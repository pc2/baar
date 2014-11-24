//===-- excerpt from ExternalFunctions.cpp - Implement External Functions --------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"

#ifdef _K1OM_
#include "ffi.h"
#else
#include <ffi.h>
#endif

typedef void (*RawFunc)();

static ffi_type *ffiTypeFor(llvm::Type *Ty) {
  switch (Ty->getTypeID()) {
    case llvm::Type::VoidTyID: return &ffi_type_void;
    case llvm::Type::IntegerTyID:
      switch (llvm::cast<llvm::IntegerType>(Ty)->getBitWidth()) {
        case 8:  return &ffi_type_sint8;
        case 16: return &ffi_type_sint16;
        case 32: return &ffi_type_sint32;
        case 64: return &ffi_type_sint64;
      }
    case llvm::Type::FloatTyID:   return &ffi_type_float;
    case llvm::Type::DoubleTyID:  return &ffi_type_double;
    case llvm::Type::PointerTyID: return &ffi_type_pointer;
    default: break;
  }
  // TODO: Support other types such as StructTyID, ArrayTyID, OpaqueTyID, etc.
  llvm::report_fatal_error("Type could not be mapped for use with libffi.");
  return NULL;
}

static void* ffiValueFor(llvm::Type *Ty, const llvm::GenericValue &AV, void *ArgDataPtr) {
  switch (Ty->getTypeID()) {
    case llvm::Type::IntegerTyID:
      switch (llvm::cast<llvm::IntegerType>(Ty)->getBitWidth()) {
        case 8: {
          int8_t *I8Ptr = (int8_t *) ArgDataPtr;
          *I8Ptr = (int8_t) AV.IntVal.getZExtValue();
          return ArgDataPtr;
        }
        case 16: {
          int16_t *I16Ptr = (int16_t *) ArgDataPtr;
          *I16Ptr = (int16_t) AV.IntVal.getZExtValue();
          return ArgDataPtr;
        }
        case 32: {
          int32_t *I32Ptr = (int32_t *) ArgDataPtr;
          *I32Ptr = (int32_t) AV.IntVal.getZExtValue();
          return ArgDataPtr;
        }
        case 64: {
          int64_t *I64Ptr = (int64_t *) ArgDataPtr;
          *I64Ptr = (int64_t) AV.IntVal.getZExtValue();
          return ArgDataPtr;
        }
      }
    case llvm::Type::FloatTyID: {
      float *FloatPtr = (float *) ArgDataPtr;
      *FloatPtr = AV.FloatVal;
      return ArgDataPtr;
    }
    case llvm::Type::DoubleTyID: {
      double *DoublePtr = (double *) ArgDataPtr;
      *DoublePtr = AV.DoubleVal;
      return ArgDataPtr;
    }
    case llvm::Type::PointerTyID: {
      void **PtrPtr = (void **) ArgDataPtr;
      *PtrPtr = GVTOP(AV);
      return ArgDataPtr;
    }
    default: break;
  }
  // TODO: Support other types such as StructTyID, ArrayTyID, OpaqueTyID, etc.
  llvm::report_fatal_error("Type value could not be mapped for use with libffi.");
  return NULL;
}

static bool ffiInvoke(RawFunc Fn, llvm::Function *F,
                      const std::vector<llvm::GenericValue> &ArgVals,
                      const llvm::DataLayout *TD, llvm::GenericValue &Result) {
  ffi_cif cif;
  llvm::FunctionType *FTy = F->getFunctionType();
  const unsigned NumArgs = F->arg_size();

  // TODO: We don't have type information about the remaining arguments, because
  // this information is never passed into ExecutionEngine::runFunction().
  if (ArgVals.size() > NumArgs && F->isVarArg()) {
    llvm::report_fatal_error("Calling external var arg function '" + F->getName()
                      + "' is not supported by the Interpreter.");
  }

  unsigned ArgBytes = 0;

  std::vector<ffi_type*> args(NumArgs);
  for (llvm::Function::const_arg_iterator A = F->arg_begin(), E = F->arg_end();
       A != E; ++A) {
    const unsigned ArgNo = A->getArgNo();
    llvm::Type *ArgTy = FTy->getParamType(ArgNo);
    args[ArgNo] = ffiTypeFor(ArgTy);
    ArgBytes += TD->getTypeStoreSize(ArgTy);
  }

  llvm::SmallVector<uint8_t, 128> ArgData;
  ArgData.resize(ArgBytes);
  uint8_t *ArgDataPtr = ArgData.data();
  llvm::SmallVector<void*, 16> values(NumArgs);
  for (llvm::Function::const_arg_iterator A = F->arg_begin(), E = F->arg_end();
       A != E; ++A) {
    const unsigned ArgNo = A->getArgNo();
    llvm::Type *ArgTy = FTy->getParamType(ArgNo);
    values[ArgNo] = ffiValueFor(ArgTy, ArgVals[ArgNo], ArgDataPtr);
    ArgDataPtr += TD->getTypeStoreSize(ArgTy);
  }

  llvm::Type *RetTy = FTy->getReturnType();
  ffi_type *rtype = ffiTypeFor(RetTy);

  if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, NumArgs, rtype, &args[0]) == FFI_OK) {
    llvm::SmallVector<uint8_t, 128> ret;
    if (RetTy->getTypeID() != llvm::Type::VoidTyID)
      ret.resize(TD->getTypeStoreSize(RetTy));
    ffi_call(&cif, Fn, ret.data(), values.data());
    switch (RetTy->getTypeID()) {
      case llvm::Type::IntegerTyID:
        switch (llvm::cast<llvm::IntegerType>(RetTy)->getBitWidth()) {
          case 8:  Result.IntVal = llvm::APInt(8 , *(int8_t *) ret.data()); break;
          case 16: Result.IntVal = llvm::APInt(16, *(int16_t*) ret.data()); break;
          case 32: Result.IntVal = llvm::APInt(32, *(int32_t*) ret.data()); break;
          case 64: Result.IntVal = llvm::APInt(64, *(int64_t*) ret.data()); break;
        }
        break;
      case llvm::Type::FloatTyID:   Result.FloatVal   = *(float *) ret.data(); break;
      case llvm::Type::DoubleTyID:  Result.DoubleVal  = *(double*) ret.data(); break;
      case llvm::Type::PointerTyID: Result.PointerVal = *(void **) ret.data(); break;
      default: break;
    }
    return true;
  }

  return false;
}
