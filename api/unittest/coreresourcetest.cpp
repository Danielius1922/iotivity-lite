/******************************************************************
 *
 * Copyright 2018 GRANITE RIVER LABS All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"),
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/

#include "api/oc_core_res_internal.h"
#include "api/oc_ri_internal.h"
#include "oc_api.h"
#include "oc_core_res.h"
#include "oc_helpers.h"
#include "port/oc_network_event_handler_internal.h"
#include "tests/gtest/Device.h"
#include "tests/gtest/RepPool.h"

#ifdef OC_HAS_FEATURE_PUSH
#include "api/oc_push_internal.h"
#endif /* OC_HAS_FEATURE_PUSH */

#include <algorithm>
#include <cstdlib>
#include <gtest/gtest.h>
#include <stdio.h>
#include <string>

static const std::string kDeviceURI{ "/oic/d" };
static const std::string kDeviceType{ "oic.d.light" };
static const std::string kDeviceName{ "Table Lamp" };
static const std::string kManufacturerName{ "Samsung" };
static const std::string kOCFSpecVersion{ "ocf.1.0.0" };
static const std::string kOCFDataModelVersion{ "ocf.res.1.0.0" };

class TestCoreResource : public testing::Test {
protected:
  void SetUp() override
  {
    oc_core_init();
    oc_network_event_handler_mutex_init();
    oc_random_init();
  }
  void TearDown() override
  {
#ifdef OC_HAS_FEATURE_PUSH
    oc_push_free();
#endif /* OC_HAS_FEATURE_PUSH */
    oc_random_destroy();
    oc_network_event_handler_mutex_destroy();
    oc_core_shutdown();
  }
};

TEST_F(TestCoreResource, InitPlatform_P)
{
  int oc_platform_info =
    oc_init_platform(kManufacturerName.c_str(), nullptr, nullptr);
  EXPECT_EQ(0, oc_platform_info);
}

TEST_F(TestCoreResource, CoreInitPlatform_P)
{
  const oc_platform_info_t *oc_platform_info =
    oc_core_init_platform(kManufacturerName.c_str(), nullptr, nullptr);
  EXPECT_EQ(kManufacturerName.length(),
            oc_string_len(oc_platform_info->mfg_name));
}

TEST_F(TestCoreResource, CoreDevice_P)
{
  oc_device_info_t *addcoredevice = oc_core_add_new_device(
    kDeviceURI.c_str(), kDeviceType.c_str(), kDeviceName.c_str(),
    kOCFSpecVersion.c_str(), kOCFDataModelVersion.c_str(), nullptr, nullptr);
  ASSERT_NE(addcoredevice, nullptr);
  size_t numcoredevice = oc_core_get_num_devices();
  EXPECT_EQ(1, numcoredevice);
  oc_connectivity_shutdown(0);
}

TEST_F(TestCoreResource, CoreGetResource_P)
{
  oc_core_init_platform(kManufacturerName.c_str(), nullptr, nullptr);

  std::string uri = "/oic/p";
  oc_resource_t *res = oc_core_get_resource_by_uri(uri.c_str(), 0);

  ASSERT_NE(nullptr, res);
  EXPECT_EQ(uri.length(), oc_string_len(res->uri));
}

static void
encodeInterfaces(unsigned iface_mask, std::vector<std::string> iface_strs = {})
{
  oc::RepPool pool{};

  oc_rep_start_root_object();
  EXPECT_EQ(CborNoError, oc_rep_get_cbor_errno());
  oc_core_encode_interfaces_mask(oc_rep_object(root), iface_mask);
  EXPECT_EQ(CborNoError, oc_rep_get_cbor_errno());
  oc_rep_end_root_object();
  EXPECT_EQ(CborNoError, oc_rep_get_cbor_errno());

  oc::oc_rep_unique_ptr rep = pool.ParsePayload();
  ASSERT_NE(nullptr, rep.get());

  if (iface_mask == 0) {
    bool is_null = false;
    EXPECT_TRUE(oc_rep_is_null(rep.get(), "if", &is_null));
    EXPECT_TRUE(is_null);
    return;
  }

  auto decode_ifaces = [](const oc_rep_t *r) {
    oc_string_array_t ifaces_out;
    size_t ifaces_out_len;
    EXPECT_TRUE(oc_rep_get_string_array(r, "if", &ifaces_out, &ifaces_out_len));

    std::vector<std::string> ifaces;
    for (size_t i = 0; i < ifaces_out_len; ++i) {
      ifaces.emplace_back(oc_string_array_get_item(ifaces_out, i));
    }
    return ifaces;
  };

  auto ifaces = decode_ifaces(rep.get());
  EXPECT_EQ(iface_strs.size(), ifaces.size());
  std::sort(std::begin(iface_strs), std::end(iface_strs));
  std::sort(std::begin(ifaces), std::end(ifaces));
  for (size_t i = 0; i < iface_strs.size(); ++i) {
    EXPECT_EQ(iface_strs[i], ifaces[i]);
  }
}

TEST_F(TestCoreResource, EncodeInterfaces_P)
{
  encodeInterfaces(0);

  std::vector<unsigned> all_ifs{
    OC_IF_BASELINE,
    OC_IF_LL,
    OC_IF_B,
    OC_IF_R,
    OC_IF_RW,
    OC_IF_A,
    OC_IF_S,
    OC_IF_CREATE,
    OC_IF_W,
    OC_IF_STARTUP,
    OC_IF_STARTUP_REVERT,
  };

  std::vector<std::string> all_ifstrs{
    OC_IF_BASELINE_STR,
    OC_IF_LL_STR,
    OC_IF_B_STR,
    OC_IF_R_STR,
    OC_IF_RW_STR,
    OC_IF_A_STR,
    OC_IF_S_STR,
    OC_IF_CREATE_STR,
    OC_IF_W_STR,
    OC_IF_STARTUP_STR,
    OC_IF_STARTUP_REVERT_STR,
  };
  ASSERT_EQ(all_ifs.size(), all_ifstrs.size());

  unsigned all_ifs_mask = 0;
  for (size_t i = 0; i < all_ifs.size(); ++i) {
    encodeInterfaces(all_ifs[i], { all_ifstrs[i] });
    all_ifs_mask |= all_ifs[i];
  }

  encodeInterfaces(all_ifs_mask, all_ifstrs);
}

class TestCoreResourceWithDevice : public testing::Test {
public:
#if defined(OC_SERVER) && defined(OC_DYNAMIC_ALLOCATION)
  static void AddDynamicResources()
  {
    oc::DynamicResourceHandler handlers{};
    handlers.onGet = oc::TestDevice::DummyHandler;

    std::vector<oc::DynamicResourceToAdd> dynResources = {
      {
        "Dynamic Device 1",
        "/dyn1",
        {
          "oic.d.dynamic",
          "oic.d.test",
        },
        {
          OC_IF_BASELINE,
          OC_IF_R,
        },
        handlers,
      },
      {
        "Dynamic Device 2",
        "/dyn2",
        {
          "oic.d.dynamic",
          "oic.d.test",
        },
        {
          OC_IF_BASELINE,
          OC_IF_RW,
        },
        handlers,
      },
    };
    size_t device = 0;
    for (const auto &dr : dynResources) {
      oc_resource_t *res = oc::TestDevice::AddDynamicResource(dr, device);
      EXPECT_NE(nullptr, res);
      device = (device + 1) % oc_core_get_num_devices();
    }
  }
#endif /* OC_SERVER && OC_DYNAMIC_ALLOCATION */

  static void SetUpTestCase()
  {
    oc::TestDevice::SetServerDevices({
      {
        /*rt=*/"oic.d.test1",
        /*name=*/"Test Device 2",
        /*spec_version=*/"ocf.1.0.0",
        /*data_model_version=*/"ocf.res.1.0.0",
        /*uri=*/"/oic/d",
      },
      {
        /*rt=*/"oic.d.test2",
        /*name=*/"Test Device 2",
        /*spec_version=*/"ocf.1.0.0",
        /*data_model_version=*/"ocf.res.1.0.0",
        /*uri=*/"/oic/d",
      },
    });
    EXPECT_TRUE(oc::TestDevice::StartServer());

#if defined(OC_SERVER) && defined(OC_DYNAMIC_ALLOCATION)
    AddDynamicResources();
#endif /* OC_SERVER && OC_DYNAMIC_ALLOCATION */
  }

  static void TearDownTestCase()
  {
    oc::TestDevice::StopServer();
  }
};

TEST_F(TestCoreResourceWithDevice, CoreGetDeviceID_F)
{
  EXPECT_EQ(nullptr, oc_core_get_device_id(SIZE_MAX));
}

TEST_F(TestCoreResourceWithDevice, CoreGetDeviceInfo_F)
{
  EXPECT_EQ(nullptr, oc_core_get_device_info(SIZE_MAX));
}

TEST_F(TestCoreResourceWithDevice, CoreGetResourceByIndex_F)
{
  EXPECT_EQ(nullptr, oc_core_get_resource_by_index(-1, /*device*/ 0));
  EXPECT_EQ(nullptr, oc_core_get_resource_by_index(OCF_D + 1, /*device*/ 0));
  EXPECT_EQ(nullptr, oc_core_get_resource_by_index(OCF_D, SIZE_MAX));
}

TEST_F(TestCoreResourceWithDevice, CoreGetResourceByURI_F)
{
  EXPECT_EQ(nullptr, oc_core_get_resource_by_uri("", 0));
  EXPECT_EQ(nullptr, oc_core_get_resource_by_uri("/oic/d", SIZE_MAX));
}

TEST_F(TestCoreResourceWithDevice, CoreGetResourceByIndex_P)
{
  auto check_resource = [](int type, size_t device) {
    oc_resource_t *res = oc_core_get_resource_by_index(type, device);
    EXPECT_NE(nullptr, res);
    const char *uri = oc_string(res->uri);
    ASSERT_NE(nullptr, uri);
    EXPECT_EQ(type, oc_core_get_resource_type_by_uri(uri));
  };

  // platform-wide resources are DCRs
  for (int type = 0; type < OCF_CON; ++type) {
    check_resource(type, /*device*/ 0);
  }

  // logical device resources
  size_t devices = oc_core_get_num_devices();
  for (size_t i = 0; i < devices; ++i) {
    for (int type = OCF_CON; type <= OCF_D; ++type) {
      check_resource(type, i);
    }
  }
}

TEST_F(TestCoreResourceWithDevice, CoreGetResourceByURI_P)
{
  auto strip_leading_slash = [](const std::string &str) {
    if (str[0] == '/') {
      return str.substr(1);
    }
    return str;
  };

  std::vector<std::string> uris{};
  for (int type = 0; type <= OCF_D; ++type) {
    oc_resource_t *res = oc_core_get_resource_by_index(type, /*device*/ 0);
    ASSERT_NE(nullptr, res) << "cannot get resource for type " << type;
    const char *res_uri = oc_string(res->uri);
    ASSERT_NE(nullptr, res_uri) << "invalid resource uri for type " << type;
    uris.push_back(strip_leading_slash(res_uri));
  }

  auto check_uri = [&strip_leading_slash](const std::string &uri,
                                          size_t device) {
    oc_resource_t *res = oc_core_get_resource_by_uri(uri.c_str(), device);
    ASSERT_NE(nullptr, res) << "cannot get resource for uri " << uri;
    const char *res_uri = oc_string(res->uri);
    ASSERT_NE(nullptr, res_uri) << "invalid uri for resource for uri " << uri;
    EXPECT_STREQ(strip_leading_slash(uri).c_str(),
                 strip_leading_slash(res_uri).c_str());
  };

  // check uris without the leading '/'
  for (const auto &uri : uris) {
    check_uri(uri, /*device*/ 0);
  }

  // add leading '/' and recheck
  std::transform(uris.cbegin(), uris.cend(), uris.begin(),
                 [](const std::string &str) { return "/" + str; });
  for (const auto &uri : uris) {
    check_uri(uri, /*device*/ 1);
  }
}

TEST_F(TestCoreResourceWithDevice, CoreGetResourceIsDCR_P)
{
  EXPECT_FALSE(oc_core_is_DCR(nullptr, 0));
  EXPECT_FALSE(
    oc_core_is_DCR(oc_core_get_resource_by_index(OCF_D, 0), SIZE_MAX));
  // platform-wide resources are DCRs
  for (int i = 0; i < OCF_CON; ++i) {
    EXPECT_TRUE(oc_core_is_DCR(oc_core_get_resource_by_index(i, 0), 0));
  }

  // logical device resources
  auto isNotDCR = [](int type) {
    return type == OCF_INTROSPECTION_WK || type == OCF_INTROSPECTION_DATA ||
           type == OCF_CON;
  };
  for (size_t device = 0; device < oc_core_get_num_devices(); ++device) {
    for (int type = OCF_CON; type <= OCF_D; ++type) {
      EXPECT_EQ(
        !isNotDCR(type),
        oc_core_is_DCR(oc_core_get_resource_by_index(type, device), device));
    }
  }

#ifdef OC_SERVER
  for (const oc_resource_t *res = oc_ri_get_app_resources(); res != nullptr;
       res = res->next) {
    EXPECT_FALSE(oc_core_is_DCR(res, res->device));
  }
#endif /* OC_SERVER */
}

#ifdef OC_SECURITY

TEST_F(TestCoreResourceWithDevice, CoreGetResourceIsSVR_P)
{
  EXPECT_FALSE(oc_core_is_SVR(nullptr, 0));
  EXPECT_FALSE(
    oc_core_is_SVR(oc_core_get_resource_by_index(OCF_D, 0), SIZE_MAX));

  // platform-wide resources are not SVRs
  for (int i = 0; i < OCF_CON; ++i) {
    EXPECT_FALSE(oc_core_is_SVR(oc_core_get_resource_by_index(i, 0), 0));
  }

  // logical device resources
  auto isSVR = [](int type) { return type >= OCF_SEC_DOXM && type < OCF_D; };
  for (size_t device = 0; device < oc_core_get_num_devices(); ++device) {
    for (int type = OCF_CON; type <= OCF_D; ++type) {
      EXPECT_EQ(
        isSVR(type),
        oc_core_is_SVR(oc_core_get_resource_by_index(type, device), device));
    }
  }

#ifdef OC_SERVER
  for (const oc_resource_t *res = oc_ri_get_app_resources(); res != nullptr;
       res = res->next) {
    EXPECT_FALSE(oc_core_is_SVR(res, res->device));
  }
#endif /* OC_SERVER */
}

#endif /* OC_SECURITY */

TEST_F(TestCoreResourceWithDevice, CoreGetResourceIsVerticalResource_P)
{
  EXPECT_FALSE(oc_core_is_vertical_resource(nullptr, 0));
  EXPECT_FALSE(oc_core_is_vertical_resource(
    oc_core_get_resource_by_index(OCF_D, 0), SIZE_MAX));

  // platform-wide resources are DCRs
  for (int i = 0; i < OCF_CON; ++i) {
    EXPECT_TRUE(
      oc_core_is_vertical_resource(oc_core_get_resource_by_index(i, 0), 0));
  }

  for (size_t device = 0; device < oc_core_get_num_devices(); ++device) {
    for (int type = OCF_CON; type <= OCF_D; ++type) {
      EXPECT_FALSE(oc_core_is_vertical_resource(
        oc_core_get_resource_by_index(type, device), device));
    }
  }

#ifdef OC_SERVER
  for (const oc_resource_t *res = oc_ri_get_app_resources(); res != nullptr;
       res = res->next) {
    EXPECT_TRUE(oc_core_is_vertical_resource(res, res->device));
  }
#endif /* OC_SERVER */
}

TEST_F(TestCoreResourceWithDevice, BindDeviceResourceType_F)
{
  oc_device_bind_resource_type(SIZE_MAX, "");
}

#ifdef OC_DYNAMIC_ALLOCATION

TEST_F(TestCoreResourceWithDevice, BindDeviceResourceType_P)
{
  const oc_endpoint_t *ep =
    oc::TestDevice::GetEndpoint(/*device*/ 0, 0, SECURED);
  ASSERT_NE(nullptr, ep);

  auto get_handler = [](oc_client_response_t *data) {
    ASSERT_EQ(OC_STATUS_OK, data->code);
    oc_string_array_t rt;
    size_t rt_len;
    oc_rep_get_string_array(data->payload, "rt", &rt, &rt_len);
    auto *rts = static_cast<std::vector<std::string> *>(data->user_data);
    for (size_t i = 0; i < rt_len; ++i) {
      rts->push_back(oc_string_array_get_item(rt, i));
    }
    oc::TestDevice::Terminate();
  };

  std::vector<std::string> rts;
  EXPECT_TRUE(
    oc_do_get("/oic/d", ep, "if=oic.if.baseline", get_handler, HIGH_QOS, &rts));
  oc::TestDevice::PoolEvents(5);
  sort(rts.begin(), rts.end());

  std::vector<std::string> exp_rts{ "oic.wk.d", "oic.d.test1" };
  sort(exp_rts.begin(), exp_rts.end());

  auto rts_equal = [](const std::vector<std::string> &lhs,
                      const std::vector<std::string> &rhs) {
    EXPECT_EQ(lhs.size(), rhs.size());
    for (size_t i = 0; i < lhs.size(); ++i) {
      EXPECT_EQ(lhs[i], rhs[i]);
    }
  };
  rts_equal(exp_rts, rts);

  std::vector<std::string> to_add_rts{
    "oic.d.test.dyn1",
    "oic.d.test.dyn2",
    "oic.d.test.dyn3",
  };
  for (const auto &add : to_add_rts) {
    oc_device_bind_resource_type(/*device*/ 0, add.c_str());
    exp_rts.push_back(add);
  }
  sort(exp_rts.begin(), exp_rts.end());

  rts.clear();
  EXPECT_TRUE(
    oc_do_get("/oic/d", ep, "if=oic.if.baseline", get_handler, HIGH_QOS, &rts));
  oc::TestDevice::PoolEvents(5);
  sort(rts.begin(), rts.end());
  rts_equal(exp_rts, rts);
}

#endif /* OC_DYNAMIC_ALLOCATION */
