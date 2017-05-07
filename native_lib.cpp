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



extern "C" int wrap1(int x) {
  return x ^ 0x20000;
}

class MyJIT {
 public:
  MyJIT() {

    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
    InitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    context_ = make_unique<LLVMContext>();
    auto module = make_unique<Module>("ti-agent-module", *context_);
    module_ = module.get();

    EngineBuilder EB(std::move(module));
    EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
    EB.setUseOrcMCJITReplacement(true);
    engine_ = std::move(std::unique_ptr<ExecutionEngine>(EB.create()));
  }

  void *gen_function() {
    FunctionType *func_type = ParseJavaSignature("(II)I", 2);
    if (func_type == nullptr) {
      return nullptr;
    }


    Function* wrap_f = llvm::Function::Create(ParseJavaSignature("(I)I"), Function::ExternalLinkage, "abs", module_);
    //engine_->addGlobalMapping(wrap_f, reinterpret_cast<void*>(::wrap1));

    Function *FooF = cast<Function>(module_->getOrInsertFunction("foo", func_type));
    std::vector<Value *>args;
    for (auto it = FooF->arg_begin(); it != FooF->arg_end(); ++it) {
      args.push_back(&*it);
    }

    BasicBlock *BB = BasicBlock::Create(*context_, "EntryBlock", FooF);
    IRBuilder<> builder(BB);

    Value *Sum = builder.CreateCall(wrap_f, std::vector<Value *>{args[2]});
    //Value *Sum = builder.CreateAdd(args[2], args[3]);

    builder.CreateRet(Sum);

    void *result = engine_->getPointerToFunction(FooF);
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
  Module* module_;
};

void *gen_function2() {
  static MyJIT jit;
  return jit.gen_function();
}


JNIEXPORT jint JNICALL Java_HelloWorld_sub (JNIEnv *env, jclass cls, jint a, jint b) {
  return sizeof(jobject);
}

JNIEXPORT jint JNICALL Java_HelloWorld_add (JNIEnv *env, jclass cls, jint a, jint b) {
  return -11;
}

JNIEXPORT jint JNICALL secret_callback (JNIEnv *env, jclass cls, jint a, jint b) {
  return 42;
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
  if (fname == "add") {
    *new_address_ptr = gen_function2();
    printf("code generated for %s\n", name_ptr);
  }
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
