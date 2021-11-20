// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <map>
#include <unordered_map>

#include "core/common/common.h"
#include "core/common/exceptions.h"
#include "core/framework/endian.h"
#include "core/framework/float16.h"
#if !defined(ORT_MINIMAL_BUILD)
#include "onnx/defs/schema.h"
#else
#include "onnx/defs/data_type_utils.h"
#endif
#include "onnx/onnx_pb.h"
#include "onnx/onnx-operators_pb.h"

struct OrtValue;

namespace ONNX_NAMESPACE {
class TypeProto;
}  // namespace ONNX_NAMESPACE

namespace onnxruntime {
/// Predefined registered types

#if !defined(DISABLE_ML_OPS)

// maps (only used by ML ops)
using MapStringToString = std::map<std::string, std::string>;
using MapStringToInt64 = std::map<std::string, int64_t>;
using MapStringToFloat = std::map<std::string, float>;
using MapStringToDouble = std::map<std::string, double>;
using MapInt64ToString = std::map<int64_t, std::string>;
using MapInt64ToInt64 = std::map<int64_t, int64_t>;
using MapInt64ToFloat = std::map<int64_t, float>;
using MapInt64ToDouble = std::map<int64_t, double>;

// vectors/sequences
using VectorMapStringToFloat = std::vector<MapStringToFloat>;
using VectorMapInt64ToFloat = std::vector<MapInt64ToFloat>;

#endif

using VectorString = std::vector<std::string>;
using VectorInt64 = std::vector<int64_t>;

// Forward declarations
class DataTypeImpl;
class TensorTypeBase;
#if !defined(DISABLE_SPARSE_TENSORS)
class SparseTensorTypeBase;
#endif
class SequenceTensorTypeBase;
class NonTensorTypeBase;
#if !defined(DISABLE_OPTIONAL_TYPE)
class OptionalTypeBase;
#endif
class PrimitiveDataTypeBase;
class Tensor;
class TensorSeq;

// DataTypeImpl pointer as unique DataTypeImpl identifier.
using MLDataType = const DataTypeImpl*;
// be used with class MLValue
using DeleteFunc = void (*)(void*);
using CreateFunc = void* (*)();

/**
 * \brief Base class for MLDataType
 *
 */
class DataTypeImpl {
 public:
  enum class GeneralType {
    kInvalid = 0,
    kNonTensor = 1,
    kTensor = 2,
    kTensorSequence = 3,
    kSparseTensor = 4,
    kOptional = 5
  };

  GeneralType type_;
  size_t size_;

 protected:
  DataTypeImpl(GeneralType type, size_t size) : type_{type}, size_{size} {}

 public:
  virtual ~DataTypeImpl() = default;

  /**
   * \brief this API will be used to check type compatibility at runtime
   *
   * \param type_proto a TypeProto instance that is constructed for a specific type
   *        will be checked against a TypeProto instance contained within a corresponding
   *        MLDataType instance.
   */
  virtual bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const = 0;

  size_t Size() const { return size_; }

  virtual DeleteFunc GetDeleteFunc() const = 0;

  /**
   * \brief Retrieves an instance of TypeProto for
   *        a given MLDataType
   * \returns optional TypeProto. Only ONNX types
              has type proto, non-ONNX types will return nullptr.
   */
  virtual const ONNX_NAMESPACE::TypeProto* GetTypeProto() const = 0;

  bool IsTensorType() const {
    return type_ == GeneralType::kTensor;
  }

  bool IsTensorSequenceType() const {
    return type_ == GeneralType::kTensorSequence;
  }

  bool IsSparseTensorType() const {
    return type_ == GeneralType::kSparseTensor;
  }

  bool IsOptionalType() const {
    return type_ == GeneralType::kOptional;
  }

  // Returns this if this is of tensor-type and null otherwise
  virtual const TensorTypeBase* AsTensorType() const {
    return nullptr;
  }

  virtual const SequenceTensorTypeBase* AsSequenceTensorType() const {
    return nullptr;
  }

#if !defined(DISABLE_SPARSE_TENSORS)
  // Returns this if this is of sparse-tensor-type and null otherwise
  virtual const SparseTensorTypeBase* AsSparseTensorType() const {
    return nullptr;
  }
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
  virtual const OptionalTypeBase* AsOptionalType() const {
    return nullptr;
  }
#endif

  virtual const NonTensorTypeBase* AsNonTensorType() const {
    return nullptr;
  }

  // Returns this if this is one of the primitive data types (specialization of PrimitiveDataTypeBase)
  // and null otherwise
  virtual const PrimitiveDataTypeBase* AsPrimitiveDataType() const {
    return nullptr;
  }

  // Return the type meta that we are using in the runtime.
  template <typename T>
  static MLDataType GetType();

  // Return the types for a concrete tensor type, like Tensor_Float
  template <typename elemT>
  static MLDataType GetTensorType();

  template <typename elemT>
  static MLDataType GetSequenceTensorType();

#if !defined(DISABLE_SPARSE_TENSORS)
  // Return the MLDataType for a concrete sparse tensor type.
  template <typename elemT>
  static MLDataType GetSparseTensorType();
#endif

  template <typename T, typename elemT>
  static MLDataType GetOptionalType();

  /**
   * Convert an ONNX TypeProto to onnxruntime DataTypeImpl.
   * However, this conversion is lossy. Don't try to use 'this->GetTypeProto()' converting it back.
   * Even though GetTypeProto() will not have the original information, it will still have enough to correctly
   * map to MLDataType.
   * \param proto
   */
  static MLDataType TypeFromProto(const ONNX_NAMESPACE::TypeProto& proto);

  static const TensorTypeBase* TensorTypeFromONNXEnum(int type);
  static const SequenceTensorTypeBase* SequenceTensorTypeFromONNXEnum(int type);
#if !defined(DISABLE_SPARSE_TENSORS)
  static const SparseTensorTypeBase* SparseTensorTypeFromONNXEnum(int type);
#endif

  static const char* ToString(MLDataType type);
  static std::vector<std::string> ToString(const std::vector<MLDataType>& types);
  // Registers ONNX_NAMESPACE::DataType (internalized string) with
  // MLDataType. DataType is produced by internalizing an instance of
  // TypeProto contained within MLDataType
  static void RegisterDataType(MLDataType);
  static MLDataType GetDataType(const std::string&);

  static const std::vector<MLDataType>& AllTensorTypes();
  static const std::vector<MLDataType>& AllFixedSizeTensorTypes();
  static const std::vector<MLDataType>& AllSequenceTensorTypes();
  static const std::vector<MLDataType>& AllFixedSizeSequenceTensorTypes();
  static const std::vector<MLDataType>& AllNumericTensorTypes();
  static const std::vector<MLDataType>& AllIEEEFloatTensorTypes();
  static const std::vector<MLDataType>& AllFixedSizeTensorExceptHalfTypes();
  static const std::vector<MLDataType>& AllIEEEFloatTensorExceptHalfTypes();
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorTypes();
  static const std::vector<MLDataType>& AllFixedSizeTensorAndSequenceTensorTypes();
  static const std::vector<MLDataType>& AllOptionalTypes();
  static const std::vector<MLDataType>& AllTensorAndSequenceTensorAndOptionalTypes();
};

std::ostream& operator<<(std::ostream& out, MLDataType data_type);

/*
 * Type registration helpers
 */
namespace data_types_internal {
/// TensorType helpers
///

template <typename T>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType();

template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<float>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT;
}
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<uint8_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT8;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<int8_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT8;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<uint16_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT16;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<int16_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT16;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<int32_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT32;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<int64_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_INT64;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<std::string>() {
  return ONNX_NAMESPACE::TensorProto_DataType_STRING;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<bool>() {
  return ONNX_NAMESPACE::TensorProto_DataType_BOOL;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<MLFloat16>() {
  return ONNX_NAMESPACE::TensorProto_DataType_FLOAT16;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<double>() {
  return ONNX_NAMESPACE::TensorProto_DataType_DOUBLE;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<uint32_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT32;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<uint64_t>() {
  return ONNX_NAMESPACE::TensorProto_DataType_UINT64;
};
template <>
constexpr ONNX_NAMESPACE::TensorProto_DataType ToTensorDataType<BFloat16>() {
  return ONNX_NAMESPACE::TensorProto_DataType_BFLOAT16;
}

// There is a specialization only for one
// type argument.
template <typename... Types>
struct TensorElementTypeSetter {
  static void SetTensorElementType(ONNX_NAMESPACE::TypeProto&);
  static void SetMapKeyType(ONNX_NAMESPACE::TypeProto&);
  static int32_t GetElementType();
};

/// Is a given type on the list of types?
/// Accepts a list of types and the first argument is the type
/// We are checking if it is listed among those that follow
template <typename T, typename... Types>
struct IsAnyOf;

/// Two types remaining, end of the list
template <typename T, typename Tail>
struct IsAnyOf<T, Tail> : public std::is_same<T, Tail> {
};

template <typename T, typename H, typename... Tail>
struct IsAnyOf<T, H, Tail...> {
  static constexpr bool value = (std::is_same<T, H>::value ||
                                 IsAnyOf<T, Tail...>::value);
};

/// Tells if the specified type is one of fundamental types
/// that can be contained within a tensor.
/// We do not have raw fundamental types, rather a subset
/// of fundamental types is contained within tensors.
template <typename T>
struct IsTensorContainedType : public IsAnyOf<T, float, uint8_t, int8_t, uint16_t, int16_t,
                                              int32_t, int64_t, std::string, bool, MLFloat16,
                                              double, uint32_t, uint64_t, BFloat16> {
};

#if !defined(DISABLE_SPARSE_TENSORS)
/// Use "IsSparseTensorContainedType<T>::value" to test if a type T
/// is permitted as the element-type of a sparse-tensor.

template <typename T>
struct IsSparseTensorContainedType : public IsAnyOf<T, float, uint8_t, int8_t, uint16_t, int16_t,
                                                    int32_t, int64_t, std::string, bool, MLFloat16,
                                                    double, uint32_t, uint64_t, BFloat16> {
};
#endif

#if !defined(DISABLE_OPTIONAL_TYPE)
/// Tells if the specified type is one of ORT types
/// that can be contained within an optional struct.
template <typename T>
struct IsOptionalOrtType : public IsAnyOf<T, Tensor, TensorSeq> {
};
#endif

/// This template's Get() returns a corresponding MLDataType
/// It dispatches the call to either GetTensorType<>() or
/// GetType<>()
template <typename T, bool TensorContainedType>
struct GetMLDataType;

template <typename T>
struct GetMLDataType<T, true> {
  static MLDataType Get() {
    return DataTypeImpl::GetTensorType<T>();
  }
};

template <typename T>
struct GetMLDataType<T, false> {
  static MLDataType Get() {
    return DataTypeImpl::GetType<T>();
  }
};

#if !defined(DISABLE_ML_OPS)
/// MapTypes helper API
/// K should always be one of the primitive data types
/// V can be either a primitive type (in which case it is a tensor)
/// or other preregistered types

void CopyMutableMapValue(const ONNX_NAMESPACE::TypeProto&,
                         ONNX_NAMESPACE::TypeProto&);

template <typename K, typename V>
struct SetMapTypes {
  static void Set(ONNX_NAMESPACE::TypeProto& proto) {
    TensorElementTypeSetter<K>::SetMapKeyType(proto);
    MLDataType dt = GetMLDataType<V, IsTensorContainedType<V>::value>::Get();
    const auto* value_proto = dt->GetTypeProto();
#ifdef ORT_NO_RTTI
    ORT_ENFORCE(value_proto != nullptr, "expected a registered ONNX type");
#else
    ORT_ENFORCE(value_proto != nullptr, typeid(V).name(),
                " expected to be a registered ONNX type");
#endif
    CopyMutableMapValue(*value_proto, proto);
  }
};
#endif

/// Sequence helpers
///
// Element type is a primitive type so we set it to a tensor<elemT>
void CopyMutableSeqElement(const ONNX_NAMESPACE::TypeProto&,
                           ONNX_NAMESPACE::TypeProto&);

// helper to create TypeProto with minimal binary size impact
struct SequenceTypeHelper {
  template <typename T>
  static const onnx::TypeProto* GetElemType() {
    MLDataType dt = GetMLDataType<T, IsTensorContainedType<T>::value>::Get();
    const auto* elem_proto = dt->GetTypeProto();
    return elem_proto;  // check for nullptr is in Set
  }

  static void Set(const onnx::TypeProto* elem_proto, ONNX_NAMESPACE::TypeProto& proto) {
    ORT_ENFORCE(elem_proto != nullptr, "expected a registered ONNX type");
    CopyMutableSeqElement(*elem_proto, proto);
  }
};

/// Optional helpers

void CopyMutableOptionalElement(const ONNX_NAMESPACE::TypeProto&,
                                ONNX_NAMESPACE::TypeProto&);

// helper to create TypeProto with minimal binary size impact
struct OptionalTypeHelper {
  template <typename T, typename elemT>
  static const onnx::TypeProto* GetElemType() {
    const onnx::TypeProto* elem_proto = nullptr;
    if (std::is_same<T, Tensor>::value) {
      MLDataType dt = DataTypeImpl::GetTensorType<elemT>();
      elem_proto = dt->GetTypeProto();
    } else if (std::is_same<T, TensorSeq>::value) {
      MLDataType dt = DataTypeImpl::GetSequenceTensorType<elemT>();
      elem_proto = dt->GetTypeProto();
    }

    return elem_proto;  // check for nullptr is in Set
  }

  static void Set(const onnx::TypeProto* elem_proto, ONNX_NAMESPACE::TypeProto& proto) {
    ORT_ENFORCE(elem_proto != nullptr, " unregistered or unsupported ORT type for Optional");
    CopyMutableOptionalElement(*elem_proto, proto);
  }
};

/// OpaqueTypes helpers
///
void AssignOpaqueDomainName(const char* domain, const char* name,
                            ONNX_NAMESPACE::TypeProto& proto);

}  // namespace data_types_internal

/// All tensors base
class TensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  /// We first compare type_proto pointers and then
  /// if they do not match try to account for the case
  /// where TypeProto was created ad-hoc and not queried from MLDataType
  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  const TensorTypeBase* AsTensorType() const override {
    return this;
  }

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(TensorTypeBase);

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  TensorTypeBase();
  ~TensorTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

/**
 * \brief Tensor type. This type does not have a C++ type associated with
 * it at registration time except the element type. One of the types mentioned
 * above at IsTensorContainedType<> list is acceptable.
 *
 * \details
 *        Usage:
 *        ORT_REGISTER_TENSOR(ELEMENT_TYPE)
 *        Currently all of the Tensors irrespective of the dimensions are mapped to Tensor<type>
 *        type. IsCompatible() currently ignores shape.
 */

template <typename elemT>
class TensorType : public TensorTypeBase {
 public:
  static_assert(data_types_internal::IsTensorContainedType<elemT>::value,
                "Requires one of the tensor fundamental types");

  static MLDataType Type();

  /// Tensors only can contain basic data types
  /// that have been previously registered with ONNXRuntime
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<elemT>();
  }

 private:
  TensorType() {
    using namespace data_types_internal;
    TensorElementTypeSetter<elemT>::SetTensorElementType(this->MutableTypeProto());
  }
};

#if defined(DISABLE_OPTIONAL_TYPE)

/// Common base-class for all disabled types. We need DataTypeImpl::ToString to work in a minimal build
/// with disabled types to keep the ORT format model kernel hashes stable.
class DisabledTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto&) const override {
    // We always want to return false for the IsCompatible() for a disabled type
    // because this will ensure that no kernel supporting the disabled type will
    // be matched to a model node requiring that type and the model load will
    // result in failure.
    return false;
  }

  DeleteFunc GetDeleteFunc() const override {
    ORT_THROW("Type is disabled in this build.");
  }

  // This must work
  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(DisabledTypeBase);

 protected:
  // This must work
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  DisabledTypeBase(DataTypeImpl::GeneralType type, size_t size);
  ~DisabledTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

#endif

#if !defined(DISABLE_SPARSE_TENSORS)
/// Common base-class for all sparse-tensors (with different element types).
class SparseTensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  const SparseTensorTypeBase* AsSparseTensorType() const override {
    return this;
  }

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(SparseTensorTypeBase);

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  SparseTensorTypeBase();
  ~SparseTensorTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};

template <typename elemT>
class SparseTensorType : public SparseTensorTypeBase {
 public:
  static_assert(data_types_internal::IsSparseTensorContainedType<elemT>::value,
                "Requires one of the sparse-tensor fundamental types");

  static MLDataType Type();

  /// Return a MLDataType representing the element-type
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<elemT>();
  }

 private:
  SparseTensorType() {
    using namespace data_types_internal;
    TensorElementTypeSetter<elemT>::SetSparseTensorElementType(MutableTypeProto());
  }
};

#endif  // !defined(DISABLE_SPARSE_TENSORS)

/// Common base-class for all optional types.

#if !defined(DISABLE_OPTIONAL_TYPE)
class OptionalTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  const OptionalTypeBase* AsOptionalType() const override {
    return this;
  }

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  DeleteFunc GetDeleteFunc() const override {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  OptionalTypeBase(const OptionalTypeBase&) = delete;
  OptionalTypeBase& operator=(const OptionalTypeBase&) = delete;

 protected:
  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  OptionalTypeBase();
  ~OptionalTypeBase() override;

 private:
  struct Impl;
  Impl* impl_;
};
#endif

// Derive from OptionalTypeBase if the Optional type support is enabled,
// else derive from DisabledTypeBase
template <typename T, typename elemT>
class OptionalType :
#if !defined(DISABLE_OPTIONAL_TYPE)
    public OptionalTypeBase
#else
    public DisabledTypeBase
#endif
{
 public:
  static MLDataType Type();

#if !defined(DISABLE_OPTIONAL_TYPE)
  static_assert(data_types_internal::IsOptionalOrtType<T>::value,
                "Requires one of the supported types: Tensor or TensorSeq");

  static_assert(data_types_internal::IsTensorContainedType<elemT>::value,
                "Requires one of the tensor fundamental types");

  MLDataType GetElementType() const override {
    if (std::is_same<T, Tensor>::value) {
      return DataTypeImpl::GetTensorType<elemT>();
    } else if (std::is_same<T, TensorSeq>::value) {
      return DataTypeImpl::GetSequenceTensorType<elemT>();
    } else {
      // Will not reach here
      ORT_ENFORCE(false, "Unsupported optional type");
    }
  }
#endif

 private:
#if !defined(DISABLE_OPTIONAL_TYPE)
  OptionalType()
#else
  OptionalType() : DisabledTypeBase { DataTypeImpl::GeneralType::kOptional, 0 }
#endif
  {
    using namespace data_types_internal;
    OptionalTypeHelper::Set(OptionalTypeHelper::GetElemType<T, elemT>(), MutableTypeProto());
  }
};  // namespace onnxruntime

/**
 * \brief Provide a specialization for your C++ Non-tensor type
 *        so your implementation FromDataTypeContainer/ToDataTypeContainer
 *        functions correctly. Otherwise you get a default implementation
 *        which may not be what you need/want.
 *
 * This class is used to create OrtValue, fetch data from OrtValue via
 * C/C++ APIs
 */
template <class T>
struct NonTensorTypeConverter {
  static void FromContainer(MLDataType /*dtype*/, const void* /*data*/, size_t /*data_size*/, OrtValue& /*output*/) {
    ORT_THROW("Not implemented");
  }
  static void ToContainer(const OrtValue& /*input*/, size_t /*data_size*/, void* /*data*/) {
    ORT_THROW("Not implemented");
  }
};

/**
 * \brief Base type for all non-tensors, maps, sequences and opaques
 */
class NonTensorTypeBase : public DataTypeImpl {
 public:
  DeleteFunc GetDeleteFunc() const override = 0;

  virtual CreateFunc GetCreateFunc() const = 0;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  const NonTensorTypeBase* AsNonTensorType() const override {
    return this;
  }

  // \brief Override for Non-tensor types to initialize non-tensor CPP
  // data representation from data. The caller of the interface
  // should have a shared definition of the data which is used to initialize
  // CPP data representation. This is used from C API.
  //
  // \param data - pointer to a data container structure non_tensor type specific
  // \param data_size - size of the data container structure, used for rudimentary checks
  // \param output - reference to a default constructed non-tensor type
  // \returns OrtValue
  // \throw if there is an error
  virtual void FromDataContainer(const void* data, size_t data_size, OrtValue& output) const;

  // \brief Override for Non-tensor types to fetch data from the internal CPP data representation
  // The caller of the interface should have a shared definition of the data which is used to initialize
  // CPP data representation. This is used from C API.
  //
  // \param input - OrtValue containing data
  // \param data_size - size of the structure that is being passed for receiving data, used for
  //                    validation
  // \param data - pointer to receiving data structure
  virtual void ToDataContainer(const OrtValue& input, size_t data_size, void* data) const;

  NonTensorTypeBase(const NonTensorTypeBase&) = delete;
  NonTensorTypeBase& operator=(const NonTensorTypeBase&) = delete;

 protected:
  NonTensorTypeBase(size_t size);
  ~NonTensorTypeBase() override;

  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

  bool IsMapCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

  bool IsSequenceCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

  bool IsOpaqueCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const;

 private:
  struct Impl;
  Impl* impl_;
};

// This is where T is the actual CPPRuntimeType
template <typename T>
class NonTensorType : public NonTensorTypeBase {
 private:
  static void Delete(void* p) {
    delete static_cast<T*>(p);
  }

 public:
  DeleteFunc GetDeleteFunc() const override {
    return &Delete;
  }

  CreateFunc GetCreateFunc() const override {
    return []() -> void* { return new T(); };
  }

 protected:
  NonTensorType() : NonTensorTypeBase(sizeof(T)) {}
};

#if !defined(DISABLE_ML_OPS)
/**
 * \brief MapType. Use this type to register
 * mapping types.
 *
 * \param T - cpp type that you wish to register as runtime MapType
 *
 * \details Usage: ORT_REGISTER_MAP(C++Type)
 *          The type is required to have mapped_type and
 *          key_type defined
 */
template <typename CPPType>
class MapType : public NonTensorType<CPPType> {
 public:
  static_assert(data_types_internal::IsTensorContainedType<typename CPPType::key_type>::value,
                "Requires one of the tensor fundamental types as key");

  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsMapCompatible(type_proto);
  }

 private:
  MapType() {
    using namespace data_types_internal;
    SetMapTypes<typename CPPType::key_type, typename CPPType::mapped_type>::Set(this->MutableTypeProto());
  }
};
#endif

/**
 * \brief SequenceType. Use to register sequence for non-tensor types.
 *
 *  \param T - CPP type that you wish to register as Sequence
 *             runtime type.
 *
 * \details Usage: ORT_REGISTER_SEQ(C++Type)
 *          The type is required to have value_type defined
 */
template <typename CPPType>
class SequenceType : public NonTensorType<CPPType> {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsSequenceCompatible(type_proto);
  }

 private:
  SequenceType() {
    using namespace data_types_internal;
    SequenceTypeHelper::Set(SequenceTypeHelper::GetElemType<typename CPPType::value_type>(),
                            this->MutableTypeProto());
  }
};

/**
 * \brief SequenceTensorTypeBase serves as a base type class for
 *        Tensor sequences. Akin TensorTypeBase.
 *        Runtime representation is always TensorSeq.
 */
class SequenceTensorTypeBase : public DataTypeImpl {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override;

  const SequenceTensorTypeBase* AsSequenceTensorType() const override {
    return this;
  }

  virtual MLDataType GetElementType() const {
    // should never reach here.
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  DeleteFunc GetDeleteFunc() const override;

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const override;

  SequenceTensorTypeBase(const SequenceTensorTypeBase&) = delete;
  SequenceTensorTypeBase& operator=(const SequenceTensorTypeBase&) = delete;

 protected:
  SequenceTensorTypeBase();
  ~SequenceTensorTypeBase();

  ONNX_NAMESPACE::TypeProto& MutableTypeProto();

 private:
  struct Impl;
  Impl* impl_;
};

/**
 * \brief SequenceTensorType. Use to register sequence for non-tensor types.
 *
 *  \param CPPRuntime - We always use TensorSeq
 *
 *  \param TensorElemType - one of the primitive types
 *
 * \details Usage: ORT_REGISTER_SEQ_TENSOR_TYPE()
 *          The type is required to have value_type defined
 */
template <typename TensorElemType>
class SequenceTensorType : public SequenceTensorTypeBase {
 public:
  static_assert(data_types_internal::IsTensorContainedType<TensorElemType>::value,
                "Requires one of the tensor fundamental types");

  static MLDataType Type();

  /// Return a MLDataType representing the element-type
  MLDataType GetElementType() const override {
    return DataTypeImpl::GetType<TensorElemType>();
  }

 private:
  SequenceTensorType() {
    using namespace data_types_internal;
    SequenceTypeHelper::Set(SequenceTypeHelper::GetElemType<TensorElemType>(), MutableTypeProto());
  }
};

/**
 * \brief OpaqueType
 *
 * \param T - cpp runtume that implements the Opaque type
 *
 * \param const char D[] - domain must be extern to be unique
 *
 * \param const char N[] - name must be extern to be unique
 *
 * \details Only one CPP type can be associated with a particular
 *          OpaqueType registration
 *
 */
template <typename T, const char D[], const char N[]>
class OpaqueType : public NonTensorType<T> {
 public:
  static MLDataType Type();

  bool IsCompatible(const ONNX_NAMESPACE::TypeProto& type_proto) const override {
    return this->IsOpaqueCompatible(type_proto);
  }

  void FromDataContainer(const void* data, size_t data_size, OrtValue& output) const override {
    NonTensorTypeConverter<T>::FromContainer(this, data, data_size, output);
  }

  void ToDataContainer(const OrtValue& input, size_t data_size, void* data) const override {
    NonTensorTypeConverter<T>::ToContainer(input, data_size, data);
  }

 private:
  OpaqueType() {
    data_types_internal::AssignOpaqueDomainName(D, N, this->MutableTypeProto());
  }
};

/**
 * \brief PrimitiveDataTypeBase
 *        Base class for primitive Tensor contained types
 *
 * \details This class contains an integer constant that can be
 *          used for input data type dispatching
 *
 */
class PrimitiveDataTypeBase : public DataTypeImpl {
 public:
  bool IsCompatible(const ONNX_NAMESPACE::TypeProto&) const override {
    return false;
  }

  const PrimitiveDataTypeBase* AsPrimitiveDataType() const override final {
    return this;
  }

  const ONNX_NAMESPACE::TypeProto* GetTypeProto() const final {
    return nullptr;
  }

  int32_t GetDataType() const {
    return data_type_;
  }

 protected:
  PrimitiveDataTypeBase(size_t size, int32_t data_type)
      : DataTypeImpl{GeneralType::kNonTensor, size}, data_type_{data_type} {}

 private:
  int32_t data_type_;
};

/**
 * \brief PrimitiveDataType
 *        Typed specialization for primitive types.
 *        Concrete instances of this class are used by Tensor.
 *
 * \param T - primitive data type
 *
 */
template <typename T>
class PrimitiveDataType : public PrimitiveDataTypeBase {
 private:
  static void Delete(void* p) {
    delete static_cast<T*>(p);
  }

 public:
  static MLDataType Type();

  DeleteFunc GetDeleteFunc() const override {
    return &Delete;
  }

 private:
  PrimitiveDataType()
      : PrimitiveDataTypeBase{sizeof(T),
                              data_types_internal::TensorElementTypeSetter<T>::GetElementType()} {
  }
};

// Explicit specialization of base class template function
// is only possible within the enclosing namespace scope,
// thus a simple way to pre-instantiate a given template
// at a registration time does not currently work and the macro
// is needed.
#define ORT_REGISTER_TENSOR_TYPE(ELEM_TYPE)             \
  template <>                                           \
  MLDataType TensorType<ELEM_TYPE>::Type() {            \
    static TensorType<ELEM_TYPE> tensor_type;           \
    return &tensor_type;                                \
  }                                                     \
  template <>                                           \
  MLDataType DataTypeImpl::GetTensorType<ELEM_TYPE>() { \
    return TensorType<ELEM_TYPE>::Type();               \
  }

#if !defined(DISABLE_SPARSE_TENSORS)
#define ORT_REGISTER_SPARSE_TENSOR_TYPE(ELEM_TYPE)            \
  template <>                                                 \
  MLDataType SparseTensorType<ELEM_TYPE>::Type() {            \
    static SparseTensorType<ELEM_TYPE> tensor_type;           \
    return &tensor_type;                                      \
  }                                                           \
  template <>                                                 \
  MLDataType DataTypeImpl::GetSparseTensorType<ELEM_TYPE>() { \
    return SparseTensorType<ELEM_TYPE>::Type();               \
  }
#endif

#define ORT_REGISTER_OPTIONAL_TYPE(ORT_TYPE, TYPE)             \
  template <>                                                  \
  MLDataType OptionalType<ORT_TYPE, TYPE>::Type() {            \
    static OptionalType<ORT_TYPE, TYPE> optional_type;         \
    return &optional_type;                                     \
  }                                                            \
  template <>                                                  \
  MLDataType DataTypeImpl::GetOptionalType<ORT_TYPE, TYPE>() { \
    return OptionalType<ORT_TYPE, TYPE>::Type();               \
  }

#if !defined(DISABLE_ML_OPS)
#define ORT_REGISTER_MAP(TYPE)               \
  template <>                                \
  MLDataType MapType<TYPE>::Type() {         \
    static MapType<TYPE> map_type;           \
    return &map_type;                        \
  }                                          \
  template <>                                \
  MLDataType DataTypeImpl::GetType<TYPE>() { \
    return MapType<TYPE>::Type();            \
  }
#endif

#define ORT_REGISTER_SEQ(TYPE)               \
  template <>                                \
  MLDataType SequenceType<TYPE>::Type() {    \
    static SequenceType<TYPE> sequence_type; \
    return &sequence_type;                   \
  }                                          \
  template <>                                \
  MLDataType DataTypeImpl::GetType<TYPE>() { \
    return SequenceType<TYPE>::Type();       \
  }

#define ORT_REGISTER_SEQ_TENSOR_TYPE(ELEM_TYPE)                 \
  template <>                                                   \
  MLDataType SequenceTensorType<ELEM_TYPE>::Type() {            \
    static SequenceTensorType<ELEM_TYPE> sequence_tensor_type;  \
    return &sequence_tensor_type;                               \
  }                                                             \
  template <>                                                   \
  MLDataType DataTypeImpl::GetSequenceTensorType<ELEM_TYPE>() { \
    return SequenceTensorType<ELEM_TYPE>::Type();               \
  }

#define ORT_REGISTER_PRIM_TYPE(TYPE)               \
  template <>                                      \
  MLDataType PrimitiveDataType<TYPE>::Type() {     \
    static PrimitiveDataType<TYPE> prim_data_type; \
    return &prim_data_type;                        \
  }                                                \
  template <>                                      \
  MLDataType DataTypeImpl::GetType<TYPE>() {       \
    return PrimitiveDataType<TYPE>::Type();        \
  }

#define ORT_REGISTER_OPAQUE_TYPE(CPPType, Domain, Name)   \
  template <>                                             \
  MLDataType OpaqueType<CPPType, Domain, Name>::Type() {  \
    static OpaqueType<CPPType, Domain, Name> opaque_type; \
    return &opaque_type;                                  \
  }                                                       \
  template <>                                             \
  MLDataType DataTypeImpl::GetType<CPPType>() {           \
    return OpaqueType<CPPType, Domain, Name>::Type();     \
  }
}  // namespace onnxruntime