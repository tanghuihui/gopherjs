#include <cstdlib>
#include <node.h>
#include <v8.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

using namespace v8;

intptr_t toNative(Local<Value> value) {
  if (value.IsEmpty()) {
    return 0;
  }
  if (value->IsArrayBufferView()) {
    Local<ArrayBufferView> view = Local<ArrayBufferView>::Cast(value);
    return *reinterpret_cast<intptr_t*>(*reinterpret_cast<char**>(*view->Buffer()) + 23) + view->ByteOffset(); // ugly hack, because of https://codereview.chromium.org/25221002
  }
  if (value->IsArray()) {
    Local<Array> array = Local<Array>::Cast(value);
    intptr_t* native = reinterpret_cast<intptr_t*>(malloc(array->Length() * sizeof(intptr_t))); // TODO memory leak
    for (uint32_t i = 0; i < array->Length(); i++) {
      native[i] = toNative(array->CloneElementAt(i));
    }
    return reinterpret_cast<intptr_t>(native);
  }
  return static_cast<intptr_t>(static_cast<int32_t>(value->ToInteger()->Value()));
}

void Syscall(const v8::FunctionCallbackInfo<Value>& info) {
  int trap = info[0]->ToInteger()->Value();
  int r1 = 0, r2 = 0;
  switch (trap) {
  case SYS_fork:
    r1 = fork();
    break;
  case SYS_pipe:
    int fd[2];
    r1 = pipe(fd);
    if (r1 == 0) {
      r1 = fd[0];
      r2 = fd[1];
    }
    break;
  default:
    r1 = syscall(
      trap,
      toNative(info[1]),
      toNative(info[2]),
      toNative(info[3])
    );
    break;
  }
  int err = 0;
  if (r1 < 0) {
    err = errno;
  }
  Local<Array> res = Array::New(3);
  res->Set(0, Integer::New(r1));
  res->Set(1, Integer::New(r2));
  res->Set(2, Integer::New(err));
  info.GetReturnValue().Set(res);
}

void Syscall6(const v8::FunctionCallbackInfo<Value>& info) {
  int r = syscall(
    info[0]->ToInteger()->Value(),
    toNative(info[1]),
    toNative(info[2]),
    toNative(info[3]),
    toNative(info[4]),
    toNative(info[5]),
    toNative(info[6])
  );
  int err = 0;
  if (r < 0) {
    err = errno;
  }
  Local<Array> res = Array::New(3);
  res->Set(0, Integer::New(r));
  res->Set(1, Integer::New(0));
  res->Set(2, Integer::New(err));
  info.GetReturnValue().Set(res);
}

void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "Syscall", Syscall);
  NODE_SET_METHOD(target, "Syscall6", Syscall6);
}

NODE_MODULE(syscall, init);