#include <dlfcn.h>
#include <jni.h>
#include <jvmti.h>

#include "llvm/Support/DynamicLibrary.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/OrcMCJITReplacement.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "HelloWorld.h"
using namespace llvm;
typedef jint (*add_ptr)(JNIEnv *, jclass, jint, jint);

int wrap_int(int x) {
  return x;
}

void* wrap_ref(void *ref) {
  return ref;
}

class Codegen {
 public:
  Codegen() {
    llvm::sys::DynamicLibrary::AddSymbol("wrap_int", reinterpret_cast<void*>(wrap_int));
    llvm::sys::DynamicLibrary::AddSymbol("wrap_ref", reinterpret_cast<void*>(wrap_ref));
    InitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    context_ = make_unique<LLVMContext>();

    EngineBuilder EB(std::move(make_unique<Module>("default_module", *context_)));
    EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
    EB.setUseOrcMCJITReplacement(true);
    EB.setErrorStr(&error_str);
    engine_ = std::move(std::unique_ptr<ExecutionEngine>(EB.create()));
  }

  void *gen_transparent_wrapper(const char *name, char *signature, void *func_ptr) {
    FunctionType *func_type = ParseJavaSignature(signature, 2);
    if (func_type == nullptr) {
      printf("Can't parse signature %s\n", signature);
      return nullptr;
    }

    auto module = make_unique<Module>(std::string(name) + "_mod", *context_);
    auto M = module.get();

    Function* wrap_ref = llvm::Function::Create(ParseJavaSignature("(L;)L;"),
                                            Function::ExternalLinkage,
                                            "wrap_ref", M);
    Function *F = cast<Function>(M->getOrInsertFunction(name, func_type));
    BasicBlock *BB = BasicBlock::Create(*context_, "", F);
    IRBuilder<> builder(BB);
    std::vector<Argument *> args;
    for (auto it = F->arg_begin(); it != F->arg_end(); ++it) {
      args.push_back(&*it);
    }

    std::vector<Value *> values;
    for (auto arg : args) {
      if (arg->getType()->isPointerTy()) {
        values.push_back(builder.CreateCall(wrap_ref, std::vector<Value *>{arg}));
      } else {
        values.push_back(arg);
      }
    }

    Value* func_value = builder.CreateIntToPtr(builder.getInt64((uint64_t)func_ptr),func_type->getPointerTo());
    Value *ret = builder.CreateCall(func_value, values);
    builder.CreateRet(ret);

    engine_->addModule(std::move(module));
    void *result = engine_->getPointerToFunction(F);
    if(result == nullptr) {
      printf("ERROR %s\n", error_str.c_str());
    }
    return result;
  }

 private:
  llvm::Type *GetPointerType() {
    return Type::getVoidTy(*context_)->getPointerTo();
  }

  FunctionType *ParseJavaSignature(const char *str, int extraPtrArgs = 0) {
    if (str == nullptr) return nullptr;
    const char* ptr = str;
    std::function<Type*()> consumeType =
      [&ptr, &consumeType, this]()->Type * {
      switch (*ptr) {
        case 'Z':
          ptr++;
          return Type::getInt8Ty(*context_);
        case 'B':
          ptr++;
          return Type::getInt8Ty(*context_);
        case 'C':
          ptr++;
          return Type::getInt16Ty(*context_);
        case 'S':
          ptr++;
          return Type::getInt16Ty(*context_);
        case 'I':
          ptr++;
          return Type::getInt32Ty(*context_);
        case 'J':
          ptr++;
          return Type::getInt64Ty(*context_);
        case 'F':
          ptr++;
          return Type::getFloatTy(*context_);
        case 'D':
          ptr++;
          return Type::getDoubleTy(*context_);
        case 'V':
          ptr++;
          return Type::getVoidTy(*context_);
        case 'L':
          while (*ptr && *ptr != ';') ptr++;
          if (*ptr == ';') {
            ptr++;
            return GetPointerType();
          } else {
            return nullptr;
          }
        case '[': {
          ptr++;
          if (consumeType())
            return GetPointerType();
          else
            return nullptr;
        }
        default:
          return nullptr;
      };
    };

    if (*ptr != '(') return nullptr;
    ptr++;
    std::vector<Type*> args;
    for (;extraPtrArgs > 0; --extraPtrArgs) {
      args.push_back(GetPointerType());
    }
    Type *returnType;
    while (*ptr && *ptr != ')') {
      auto type = consumeType();
      if (type == nullptr) return nullptr;
      args.push_back(type);
    }
    if (*ptr != ')') return nullptr;
    ptr++;
    returnType = consumeType();
    if (returnType == nullptr || *ptr) return nullptr;

    return FunctionType::get(returnType, args, false);

  }

  std::unique_ptr<LLVMContext> context_;
  std::unique_ptr<ExecutionEngine> engine_;
  std::string error_str;
};

void *gen_function(char* name_base, char *signature, void *func_ptr) {
  static std::mutex g_mutex;
  std::lock_guard<std::mutex> lock(g_mutex);
  static Codegen codegen;
  static int n = 0;
  std::string name = std::string(name_base) + "_ti_" + std::to_string(n++);
  // printf("Signature: %s\n", signature);
  void *result = codegen.gen_transparent_wrapper(name.c_str(), signature, func_ptr);
  // printf("code generated for %s = %p\n", name.c_str(), result);
  return result;
}


JNIEXPORT jint JNICALL Java_HelloWorld_add (JNIEnv *env, jclass cls, jint a, jint b) {
  printf("Real add was called with args %d %d\n", a, b);
  return -11;
}

jvmtiEnv* CreateJvmtiEnv(JavaVM* vm) {
  jvmtiEnv* jvmti_env;
  jint result = vm->GetEnv((void**)&jvmti_env, JVMTI_VERSION_1_2);
  if (result != JNI_OK) {
    printf("error1\n");
    return nullptr;
  }
  return jvmti_env;
}

void JNICALL
NativeMethodBind(jvmtiEnv *ti,
            JNIEnv* jni_env,
            jthread thread,
            jmethodID method,
            void* address,
            void** new_address_ptr) {
  char* name_ptr = nullptr;
  char* signature_ptr = nullptr;
  ti->GetMethodName(method, &name_ptr, &signature_ptr, nullptr);
  std::string fname (name_ptr);
  // for (int i = 0; i < 20; ++i) {
    *new_address_ptr = gen_function(name_ptr, signature_ptr, address);
  // }
  ti->Deallocate((unsigned char *)name_ptr);
  ti->Deallocate((unsigned char *)signature_ptr);
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  jvmtiError error;

  jvmtiEnv* ti = CreateJvmtiEnv(vm);

  // Hook up event callbacks
  jvmtiEventCallbacks callbacks = {};
  callbacks.NativeMethodBind = &NativeMethodBind;
  error = ti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (error != JNI_OK) {
    printf("error2\n");
  }
  jvmtiCapabilities caps;
  error = ti->GetPotentialCapabilities(&caps);
  error = ti->AddCapabilities(&caps);
  error = ti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_NATIVE_METHOD_BIND, nullptr);
  if (error != JNI_OK) {
    printf("error3\n");
  }

  return 0;
}
