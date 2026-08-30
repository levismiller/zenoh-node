#include <napi.h>
#include <memory>
#include <string>
#include <vector>
#include "zenoh.hxx"
#include "zenoh/api/ext/serialization.hxx"

using namespace zenoh;

struct QueryReplyData {
    Query query;
    std::string keyexpr;
    std::string params;
    bool replied = false;
};

class ZenohSession : public Napi::ObjectWrap<ZenohSession> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "Session", {
            InstanceMethod("put",              &ZenohSession::Put),
            InstanceMethod("subscribe",        &ZenohSession::Subscribe),
            InstanceMethod("declareQueryable", &ZenohSession::DeclareQueryable),
            InstanceMethod("get",              &ZenohSession::Get),
            InstanceMethod("close",            &ZenohSession::Close),
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
        } else if (info.Length() > 0 && info[0].IsObject()) {
            Napi::Object opts = info[0].As<Napi::Object>();
            if (opts.Has("connect") && opts.Get("connect").IsString()) {
                std::string locator = opts.Get("connect").As<Napi::String>().Utf8Value();
                config.insert_json5("connect/endpoints", ("[\"" + locator + "\"]").c_str(), &err);
                if (err != Z_OK) {
                    Napi::Error::New(env, "Invalid connect locator").ThrowAsJavaScriptException();
                    return;
                }
            }
            if (opts.Has("listen") && opts.Get("listen").IsString()) {
                std::string locator = opts.Get("listen").As<Napi::String>().Utf8Value();
                config.insert_json5("listen/endpoints", ("[\"" + locator + "\"]").c_str(), &err);
                if (err != Z_OK) {
                    Napi::Error::New(env, "Invalid listen locator").ThrowAsJavaScriptException();
                    return;
                }
            }
        }
        session_ = std::make_unique<Session>(std::move(config), Session::SessionOptions::create_default(), &err);
        if (err != Z_OK) {
            Napi::Error::New(env, "Failed to open Zenoh session").ThrowAsJavaScriptException();
        }
    }

    ~ZenohSession() { cleanup(); }

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
        std::string keyexpr = info[0].As<Napi::String>().Utf8Value();
        std::string payload = info[1].As<Napi::String>().Utf8Value();
        ZResult err;
        session_->put(KeyExpr(keyexpr), ext::serialize(payload), Session::PutOptions::create_default(), &err);
        if (err != Z_OK) {
            Napi::Error::New(env, "Zenoh put failed").ThrowAsJavaScriptException();
        }
        return env.Undefined();
    }

    Napi::Value Subscribe(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
            Napi::TypeError::New(env, "subscribe(keyexpr: string, callback: Function)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        auto tsfn = Napi::ThreadSafeFunction::New(
            env, info[1].As<Napi::Function>(), "ZenohSubscriber", 0, 1, [](Napi::Env) {}
        );
        ZResult err;
        auto sub = session_->declare_subscriber(
            KeyExpr(info[0].As<Napi::String>().Utf8Value()),
            [tsfn](Sample& sample) {
                std::string key(sample.get_keyexpr().as_string_view());
                std::string payload = ext::deserialize<std::string>(sample.get_payload());
                auto* d = new std::pair<std::string, std::string>(std::move(key), std::move(payload));
                tsfn.NonBlockingCall(d, [](Napi::Env env, Napi::Function cb, std::pair<std::string, std::string>* d) {
                    cb.Call({ Napi::String::New(env, d->first), Napi::String::New(env, d->second) });
                    delete d;
                });
            },
            []() {},
            Session::SubscriberOptions::create_default(),
            &err
        );
        if (err != Z_OK) {
            tsfn.Release();
            Napi::Error::New(env, "Failed to declare subscriber").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        subscribers_.push_back(std::move(sub));
        tsfns_.push_back(std::move(tsfn));
        return env.Undefined();
    }

    Napi::Value DeclareQueryable(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
            Napi::TypeError::New(env, "declareQueryable(keyexpr: string, callback: Function)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        auto tsfn = Napi::ThreadSafeFunction::New(
            env, info[1].As<Napi::Function>(), "ZenohQueryable", 0, 1, [](Napi::Env) {}
        );
        ZResult err;
        auto queryable = session_->declare_queryable(
            KeyExpr(info[0].As<Napi::String>().Utf8Value()),
            [tsfn](Query& query) {
                std::string key(query.get_keyexpr().as_string_view());
                std::string params(query.get_parameters());
                auto* d = new QueryReplyData{ query.clone(), key, params, false };
                tsfn.NonBlockingCall(d, [](Napi::Env env, Napi::Function cb, QueryReplyData* d) {
                    Napi::Function reply_fn = Napi::Function::New(
                        env,
                        [](const Napi::CallbackInfo& info) -> Napi::Value {
                            auto* data = static_cast<QueryReplyData*>(info.Data());
                            if (info.Length() >= 1 && info[0].IsString() && !data->replied) {
                                std::string payload = info[0].As<Napi::String>().Utf8Value();
                                ZResult err;
                                data->query.reply(
                                    KeyExpr(data->keyexpr), ext::serialize(payload),
                                    Query::ReplyOptions::create_default(), &err
                                );
                                data->replied = true;
                            }
                            return info.Env().Undefined();
                        },
                        "reply",
                        d
                    );
                    napi_add_finalizer(
                        env, reply_fn, d,
                        [](napi_env, void* raw, void*) { delete static_cast<QueryReplyData*>(raw); },
                        nullptr, nullptr
                    );
                    cb.Call({
                        Napi::String::New(env, d->keyexpr),
                        Napi::String::New(env, d->params),
                        reply_fn
                    });
                });
            },
            []() {},
            Session::QueryableOptions::create_default(),
            &err
        );
        if (err != Z_OK) {
            tsfn.Release();
            Napi::Error::New(env, "Failed to declare queryable").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        queryables_.push_back(std::move(queryable));
        tsfns_.push_back(std::move(tsfn));
        return env.Undefined();
    }

    // get(keyexpr, params, onReply(keyexpr, payload), onDone?())
    Napi::Value Get(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();
        if (!session_) {
            Napi::Error::New(env, "Session is closed").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        if (info.Length() < 3 || !info[0].IsString() || !info[1].IsString() || !info[2].IsFunction()) {
            Napi::TypeError::New(env, "get(keyexpr: string, params: string, onReply: Function, onDone?: Function)").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        std::string keyexpr = info[0].As<Napi::String>().Utf8Value();
        std::string params  = info[1].As<Napi::String>().Utf8Value();
        auto reply_tsfn = Napi::ThreadSafeFunction::New(
            env, info[2].As<Napi::Function>(), "ZenohGetReply", 0, 1, [](Napi::Env) {}
        );

        auto on_reply = [reply_tsfn](Reply& reply) {
            if (!reply.is_ok()) return;
            const Sample& sample = reply.get_ok();
            std::string key(sample.get_keyexpr().as_string_view());
            std::string payload = ext::deserialize<std::string>(sample.get_payload());
            auto* d = new std::pair<std::string, std::string>(std::move(key), std::move(payload));
            reply_tsfn.NonBlockingCall(d, [](Napi::Env env, Napi::Function cb, std::pair<std::string, std::string>* d) {
                cb.Call({ Napi::String::New(env, d->first), Napi::String::New(env, d->second) });
                delete d;
            });
        };

        ZResult err;
        bool has_done = info.Length() >= 4 && info[3].IsFunction();
        if (has_done) {
            auto done_tsfn = Napi::ThreadSafeFunction::New(
                env, info[3].As<Napi::Function>(), "ZenohGetDone", 0, 1, [](Napi::Env) {}
            );
            session_->get(KeyExpr(keyexpr), params, on_reply,
                [reply_tsfn, done_tsfn]() mutable {
                    done_tsfn.NonBlockingCall([](Napi::Env env, Napi::Function cb) { cb.Call({}); });
                    done_tsfn.Release();
                    reply_tsfn.Release();
                },
                Session::GetOptions::create_default(), &err
            );
            if (err != Z_OK) done_tsfn.Release();
        } else {
            session_->get(KeyExpr(keyexpr), params, on_reply,
                [reply_tsfn]() mutable { reply_tsfn.Release(); },
                Session::GetOptions::create_default(), &err
            );
        }

        if (err != Z_OK) {
            reply_tsfn.Release();
            Napi::Error::New(env, "Zenoh get failed").ThrowAsJavaScriptException();
        }
        return env.Undefined();
    }

    Napi::Value Close(const Napi::CallbackInfo& info) {
        cleanup();
        return info.Env().Undefined();
    }

private:
    std::unique_ptr<Session> session_;
    std::vector<Subscriber<void>>  subscribers_;
    std::vector<Queryable<void>>   queryables_;
    std::vector<Napi::ThreadSafeFunction> tsfns_;

    void cleanup() {
        subscribers_.clear();
        queryables_.clear();
        for (auto& tsfn : tsfns_) tsfn.Release();
        tsfns_.clear();
        session_.reset();
    }
};

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    ZenohSession::Init(env, exports);
    return exports;
}

NODE_API_MODULE(zenoh_node, Init)
