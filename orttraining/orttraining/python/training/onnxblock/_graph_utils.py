# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# _graph_utils.py

import copy
import onnx
import random

from onnxruntime.capi._pybind_state import GradientGraphBuilder


def get_output_from_output_name(onnx_model, output_name):
    """Returns the graph output given the output name"""

    # Iterate over the graph outputs looking for output_name
    for output in onnx_model.graph.output:
        if output.name == output_name:
            return output

    raise LookupError(f"The provided output name {output_name} is not a graph output.")


def get_random_number():
    """Return a random number in the range 0, 1000."""

    return random.randint(0, 1000)


def generate_random_graph_name(token):
    """Return a string that can be used in the graph as a graph attribute name."""

    return f"{get_random_number()}.{token}"


def build_gradient_graph(
    accessor, user_args_requiring_grad, user_args_not_requiring_grad, output_names
):
    """Builds the gradient graph on top of the given input forward only graph."""

    model = accessor.model

    # Collect names of parameters that need gradients computed
    all_args_requiring_gradient = set()
    # Move all trainable and non trainable initializers to graph inputs.
    # This allows training to pass in the parameters from outside the graph
    # so as to share the parameters across multiple sessions.
    graph_inputs = model.graph.input
    initializers = []
    for initializer in model.graph.initializer:
        if not initializer.name[0].isdigit():
            # Move only those initializers as inputs that are not local
            # to the onnx model. i.e. initializers that are model parameters.
            # These are tpically those initializers without any number prefixed
            # to their names.
            graph_inputs.append(
                onnx.helper.make_tensor_value_info(
                    initializer.name, initializer.data_type, initializer.dims
                )
            )
            if initializer.name not in user_args_not_requiring_grad:
                all_args_requiring_gradient.add(initializer.name)
        else:
            # All other initializers stay where they were.
            initializers.append(initializer)

    # Update the initializers in the graph
    del model.graph.initializer[:]
    model.graph.initializer.extend(initializers)

    # At this point, we have the eval model
    accessor.eval_model = copy.deepcopy(model)

    # Any graph input that requires gradient, should have been already added to
    # args_requiring_grad. So, add these arguments to set of arguments
    # whose gradient should be built.
    for argument_name in user_args_requiring_grad:
        all_args_requiring_gradient.add(argument_name)

    # Assumption is that the first graph output is the loss output
    if isinstance(output_names, str):
        output_names = [output_names]
    builder = GradientGraphBuilder(
        model.SerializeToString(),
        set(output_names),
        all_args_requiring_gradient,
        output_names[0],
    )
    builder.build()
    gradient_model = onnx.load_from_string(builder.get_model())

    # copy the gradient graph into the user's graph
    model.graph.CopyFrom(gradient_model.graph)
    del model.opset_import[:]
    model.opset_import.extend(gradient_model.opset_import)

    return all_args_requiring_gradient


def build_gradient_accumulation_graph(grad_model, all_args_requiring_gradient_names):
    """Builds gradient accumulation nodes on top of a training model.

    Adds an InPlaceAccumulator node for every gradient so that the gradients
    are accumulated in a gradient buffer (which is an input to InPlaceAccumulator).

    For example, if there is a gradient in the graph called fc1.weight_grad,
    an InPlaceAccumulator will be added for that gradient whose input will
    be a graph input (fc1.weight_grad.accumulation.buffer) and the newly
    computed gradient (fc1.weight_grad).

    gradient.accumulation.buffer        gradient
        |         |                         |
        |         v                         v
        |         |_________________________|
        |                      |
        |               InPlaceAccumulator
        |                      |
        |                      v
        |______________________|
    """

    # TODO: Avoid hard coded input/output strings
    gradient_output_names = {
        f"{arg_name}_grad" for arg_name in all_args_requiring_gradient_names
    }

    graph_inputs = grad_model.graph.input
    graph_nodes = grad_model.graph.node
    graph_outputs = []

    lazy_reset_grad_input_name = "lazy_reset_grad"
    gradient_output_suffix = "_grad"
    gradient_accumulation_name = "accumulation"
    gradient_buffer_name_suffix = "buffer"
    gradient_output_name_suffix = "out"

    for idx, graph_output in enumerate(grad_model.graph.output):
        if graph_output.name not in gradient_output_names:
            # If graph output is not a gradient, there is no
            # need to build the gradient accumulator node for it.
            graph_outputs.append(graph_output)
            continue

        # gradient accumulation node inputs and output names
        grad_name = graph_output.name
        grad_accumulation_buffer_name = (
            f"{grad_name}.{gradient_accumulation_name}.{gradient_buffer_name_suffix}"
        )
        grad_accumulation_output_name = (
            f"{grad_name}.{gradient_accumulation_name}.{gradient_output_name_suffix}"
        )

        # Gradient accumulation node
        acc_node = onnx.helper.make_node(
            "InPlaceAccumulator",
            [grad_accumulation_buffer_name, grad_name, lazy_reset_grad_input_name],
            [grad_accumulation_output_name],
            name=f"GradientAccumulator{idx}",
            domain="com.microsoft",
        )

        graph_nodes.append(acc_node)

        # grad buffer is also a graph input
        grad_accumulation_buffer_input = copy.deepcopy(graph_output)
        grad_accumulation_buffer_input.name = grad_accumulation_buffer_name
        graph_inputs.append(grad_accumulation_buffer_input)

        # accumulated gradient is also a graph output
        grad_accumulation_output = copy.deepcopy(graph_output)
        grad_accumulation_output.name = grad_accumulation_output_name
        graph_outputs.append(grad_accumulation_output)

    lazy_reset_grad_input = onnx.helper.make_tensor_value_info(
        lazy_reset_grad_input_name, onnx.TensorProto.BOOL, [1]
    )
    graph_inputs.append(lazy_reset_grad_input)

    del grad_model.graph.output[:]
    grad_model.graph.output.extend(graph_outputs)


def get_model_parameters(model, args_not_requiring_gradient):
    """Returns trainable and non trainable onnx model parameters."""

    trainable_params = []
    non_trainable_params = []
    for initializer in model.graph.initializer:
        # All model parameters should have their names not begin with
        # a digit. So, check to see if the initializer's first char
        # is a digit. If not, it is either a trainable, or a non
        # trainable parameter.
        # Note that this assumption can be made because the export logic
        # does not change the names of the original model parameters.
        # and the original model parameters don't have their names begin
        # with a digit.
        # On the other hand, const initializers are generated by export
        # logic and have a digit prefix.
        # TODO: validate this assumption. If assumption is not valid,
        # the alternative is to enforce the user to provide the parameter names.
        if not initializer.name[0].isdigit():
            if initializer.name in args_not_requiring_gradient:
                non_trainable_params.append(initializer)
            else:
                trainable_params.append(initializer)

    return trainable_params, non_trainable_params