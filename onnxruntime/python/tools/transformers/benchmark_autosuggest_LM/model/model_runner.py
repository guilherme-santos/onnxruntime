import torch
import argparse

from generate import LMGenerator
from wrapper.onnx import OnnxModelWrapper
from tokenizer import Tokenizer

class ModelRunner:
    def __init__(self) -> None:
        pass

    def process_results(
        self,
        output_ids: torch.Tensor,
        output_probs: torch.Tensor,
        tokenizer: Tokenizer,
        last_complete_word_position: int):

        #TODO why is this expanding to a 3D tensor
        if output_ids.dim() == 2: # one output
            output_ids = output_ids[:, None]

        returns = []
        returns_probs = []
        for i, output_i in enumerate(output_ids):
            returns_i = []
            probs_i = []
            for j, output_ij in enumerate(output_i):
                # TODO is args.sequence_prob_threshold really needed to be passed for zero?
                if returns_i and output_probs[i, j] < 0:
                    break
                output_ij = output_ij.tolist()
                for k in range(len(output_ij)):
                    if output_ij[k] == tokenizer.eos_token_id:
                        output_ij = output_ij[:k]
                        break

                output_ij = output_ij[last_complete_word_position:]
                output_text = tokenizer.decode(output_ij).rstrip()
                if output_text not in returns_i:
                    returns_i.append(output_text)
                    probs_i.append(output_probs[i, j].item())

            returns.append(returns_i)
            returns_probs.append(probs_i)

        return returns, returns_probs


    @torch.no_grad()
    def autocomplete(
        self,
        args : argparse,
        model : OnnxModelWrapper,
        tokenizer : Tokenizer,
        input_text : str,
        pad_token_id : int,
        is_onnx_model = False):

        input_ids, first_token_masks, last_complete_word_position = tokenizer.encode_text_with_partial_word(input_text, pad_token_id, args.device)

        # This is created for each query, currently there is no performance impact for this.
        generator = LMGenerator(
            max_length=input_ids.size(1) + args.num_words,
            num_return_sequences=args.num_suggestions,
            num_beams=args.num_beams,
            pad_token_id=tokenizer.eos_token_id,
            eos_token_ids=[tokenizer.eos_token_id],
            length_penalty=args.length_penalty,
            enable_ort = is_onnx_model)

        output_ids, output_probs = generator.generate(model, input_ids, first_token_masks=first_token_masks)

        return self.process_results(output_ids, output_probs, tokenizer, last_complete_word_position)