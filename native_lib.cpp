#include <jni.h>
#include <jvmti.h>

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
typedef jint (*add_ptr)(JNIEnv *, jclass, jint, jint);

void* gen_function() {
  using namespace llvm;
  InitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  LLVMContext *pContext = new LLVMContext();
  LLVMContext &Context = *pContext;

  // Create some module to put our function into it.
  std::unique_ptr<Module> Owner = make_unique<Module>("test", Context);
  Module *M = Owner.get();

  // Now we're going to create function `foo', which returns an int and takes no
  // arguments.
  Function *FooF =
    cast<Function>(M->getOrInsertFunction("foo", Type::getInt32Ty(Context),
      Type::getDoublePtrTy(Context), Type::getDoublePtrTy(Context),
          Type::getInt32Ty(Context), Type::getInt32Ty(Context), nullptr));

  // Add a basic block to the FooF function.
  BasicBlock *BB = BasicBlock::Create(Context, "EntryBlock", FooF);

  // Create a basic block builder with default parameters.  The builder will
  // automatically append instructions to the basic block `BB'.
  IRBuilder<> builder(BB);

  // Tell the basic block builder to attach itself to the new basic block
  builder.SetInsertPoint(BB);

  // Get pointer to the constant `10'.
  Value *Ten = builder.getInt32(10);

  // Create the return instruction and add it to the basic block.
  builder.CreateRet(Ten);

  // Now we create the JIT.
  EngineBuilder EB(std::move(Owner));
  EB.setMCJITMemoryManager(make_unique<SectionMemoryManager>());
  EB.setUseOrcMCJITReplacement(true);
  ExecutionEngine* EE = EB.create();
  EE->finalizeObject();

  //delete EE;
  //llvm_shutdown();
  void *result = EE->getPointerToFunction(FooF);
  ((add_ptr)result)(nullptr, nullptr, 3, 4);
  return result;
}

JNIEXPORT jint JNICALL Java_HelloWorld_sub (JNIEnv *env, jclass cls, jint a, jint b) {
  return sizeof(jobject);
}

JNIEXPORT jint JNICALL Java_HelloWorld_add (JNIEnv *env, jclass cls, jint a, jint b) {
  static JNINativeMethod m;
  m.name = (char*)"add";
  m.signature = (char*)"(II)I";
  m.fnPtr = (void*)Java_HelloWorld_sub;
  env->RegisterNatives(cls, &m, 1);
  return a + b;
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
  jvmtiError error = ti->GetMethodName(method, &name_ptr, &signature_ptr, nullptr);
  std::string fname (name_ptr);
  if (fname == "add") {
    *new_address_ptr = gen_function();
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
