// Copyright 2025 KVCache.AI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tent/transport/rdma/ibv_loader.h"

#include <dlfcn.h>
#include <glog/logging.h>

namespace mooncake {
namespace tent {

template <typename Fn>
bool LoadSymbol(void* handle, const char* name, Fn& out) {
    void* sym = dlsym(handle, name);
    if (!sym) {
        LOG(WARNING) << "libibverbs missing required symbol: " << name
                     << ", error: " << dlerror();
        return false;
    }
    out = reinterpret_cast<Fn>(sym);
    return true;
}

IbvLoader& IbvLoader::Instance() {
    static IbvLoader instance;
    return instance;
}

IbvLoader::IbvLoader() {
    // Prefer the copy already mapped by DT_NEEDED (cpu/cuda topology probe
    // used to call ibv_* directly). A second dlopen of a different
    // libibverbs.so.1 (conda vs MetaX) constructs libkfi twice; spdlog then
    // aborts with "logger with name 'KFI' already exists".
    dlerror();
    if (dlsym(RTLD_DEFAULT, "ibv_get_device_list") != nullptr) {
        handle_ = RTLD_DEFAULT;
        owned_handle_ = false;
        LOG(INFO) << "IbvLoader: reusing process-global libibverbs";
    } else {
        dlerror();
        handle_ = dlopen("libibverbs.so.1", RTLD_NOLOAD | RTLD_GLOBAL);
        if (handle_) {
            owned_handle_ = false;
            LOG(INFO) << "IbvLoader: reusing already-loaded libibverbs.so.1";
        } else {
            dlerror();
            handle_ = dlopen("libibverbs.so.1", RTLD_NOW | RTLD_GLOBAL);
            if (!handle_) {
                LOG(INFO) << "IbvLoader: libibverbs.so.1 not available: "
                          << dlerror();
                return;
            }
            owned_handle_ = true;
        }
    }

    bool ok = true;
    ok &= LoadSymbol(handle_, "ibv_get_device_list",
                     symbols_.ibv_get_device_list);
    ok &= LoadSymbol(handle_, "ibv_free_device_list",
                     symbols_.ibv_free_device_list);
    ok &= LoadSymbol(handle_, "ibv_open_device", symbols_.ibv_open_device);
    ok &= LoadSymbol(handle_, "ibv_close_device", symbols_.ibv_close_device);
    ok &= LoadSymbol(handle_, "ibv_query_device", symbols_.ibv_query_device);
    ok &= LoadSymbol(handle_, "ibv_query_gid", symbols_.ibv_query_gid);
    ok &=
        LoadSymbol(handle_, "ibv_query_port", symbols_.ibv_query_port_default);
    ok &= LoadSymbol(handle_, "ibv_get_device_name",
                     symbols_.ibv_get_device_name);

    ok &= LoadSymbol(handle_, "ibv_alloc_pd", symbols_.ibv_alloc_pd);
    ok &= LoadSymbol(handle_, "ibv_dealloc_pd", symbols_.ibv_dealloc_pd);

    ok &= LoadSymbol(handle_, "ibv_create_comp_channel",
                     symbols_.ibv_create_comp_channel);
    ok &= LoadSymbol(handle_, "ibv_destroy_comp_channel",
                     symbols_.ibv_destroy_comp_channel);

    ok &= LoadSymbol(handle_, "ibv_create_cq", symbols_.ibv_create_cq);
    ok &= LoadSymbol(handle_, "ibv_destroy_cq", symbols_.ibv_destroy_cq);

    ok &= LoadSymbol(handle_, "ibv_create_qp", symbols_.ibv_create_qp);
    ok &= LoadSymbol(handle_, "ibv_destroy_qp", symbols_.ibv_destroy_qp);
    ok &= LoadSymbol(handle_, "ibv_modify_qp", symbols_.ibv_modify_qp);

    ok &= LoadSymbol(handle_, "ibv_get_cq_event", symbols_.ibv_get_cq_event);
    ok &= LoadSymbol(handle_, "ibv_ack_cq_events", symbols_.ibv_ack_cq_events);

    ok &= LoadSymbol(handle_, "ibv_reg_mr", symbols_.ibv_reg_mr_default);
    ok &= LoadSymbol(handle_, "ibv_reg_mr_iova2", symbols_.ibv_reg_mr_iova2);
    ok &= LoadSymbol(handle_, "ibv_dereg_mr", symbols_.ibv_dereg_mr);

    ok &= LoadSymbol(handle_, "ibv_fork_init", symbols_.ibv_fork_init);

    if (!ok) {
        LOG(WARNING) << "IbvLoader: missing required verbs symbols, "
                     << "RDMA will be disabled.";
        if (owned_handle_ && handle_) {
            dlclose(handle_);
        }
        handle_ = nullptr;
        owned_handle_ = false;
        return;
    }

    // Device presence is decided by topology probe / RdmaTransport, not here.
    // Calling ibv_get_device_list in this ctor used to be a second KFI init
    // on MetaX after CudaPlatform::probe already enumerated NICs.
    available_ = true;
    LOG(INFO) << "IbvLoader: libibverbs loaded successfully";
}

IbvLoader::~IbvLoader() {
    if (owned_handle_ && handle_) {
        dlclose(handle_);
    }
    handle_ = nullptr;
    owned_handle_ = false;
}
}  // namespace tent
}  // namespace mooncake
