#include <napi.h>
#include <memory>
#include <string>
#include "zenoh.hxx"
#include "zenoh/api/ext/serialization.hxx"

using namespace zenoh;

class ZenohSession : public Napi::ObjectWrap<ZenohSession> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Session", {
            InstanceMethod("put",   &ZenohSession::Put),
            InstanceMethod("close", &ZenohSession::Close),
        });
        Napi::FunctionReference* constructor = new Napi::FunctionReference();
        *constructor = Napi::Persistent(func);
        env.SetInstanceData(constructor);
        exports.Set("Session", func);
        return exports;
    }

    ZenohSession(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ZenohSession>(info) {
        Napi::Env env = info.Env();

        ZResult err;
        Config config = Config::create_default();

        if (info.Length() > 0 && info[0].IsString()) {
            std::string locator = info[0].As<Napi::String>().Utf8Value();
            config.insert_json5("connect/endpoints", ("[\"" + locator + "\"]").c_str(), &err);
            if (err != Z_OK) {
                Napi::Error::New(env, "Invalid locator").ThrowAsJavaScriptException();
                return;
            }
        }

        session_ = std::make_unique<Session>(std::move(config), Session::SessionOptions::create_default(), &err);
        if (err != Z_OK) {
            Napi::Error::New(env, "Failed to open Zenoh session").ThrowAsJavaScriptException();
        }
    }

    Napi::Value Put(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
            Napi::TypeError::New(env, "put(keyexpr: string, payload: string)").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        std::string keyexpr  = info[0].As<Napi::String>().Utf8Value();
        std::string payload  = info[1].As<Napi::String>().Utf8Value();

        ZResult err;
        session_->put(KeyExpr(keyexpr), ext::serialize(payload), Session::PutOptions::create_default(), &err);
        if (err != Z_OK) {
            Napi::Error::New(env, "Zenoh put failed").ThrowAsJavaScriptException();
        }
        return env.Undefined();
    }

    Napi::Value Close(const Napi::CallbackInfo& info) {
        session_.reset();
        return info.Env().Undefined();
    }

private:
    std::unique_ptr<Session> session_;
};

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    ZenohSession::Init(env, exports);
    return exports;
}

NODE_API_MODULE(zenoh_node, Init)
