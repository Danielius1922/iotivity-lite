/****************************************************************************
 *
 * Copyright (c) 2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License"),
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include "oc_api.h"
#include "oc_pki.h"
#include "port/oc_clock.h"
#include "security/oc_certs_internal.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>

pthread_mutex_t mutex;
pthread_cond_t cv;
struct timespec ts;

int quit = 0;

static bool state = false;
int power;
oc_string_t name;

static int
app_init(void)
{
  int ret = oc_init_platform("Intel", NULL, NULL);
  ret |= oc_add_device("/oic/d", "oic.d.light", "Lamp", "ocf.2.0.0",
                       "ocf.res.2.0.0", NULL, NULL);
  oc_new_string(&name, "John's Light", 12);
  return ret;
}

static void
get_light(oc_request_t *request, oc_interface_mask_t iface_mask,
          void *user_data)
{
  (void)user_data;
  ++power;

  PRINT("GET_light:\n");
  oc_rep_start_root_object();
  switch (iface_mask) {
  case OC_IF_BASELINE:
    oc_process_baseline_interface(request->resource);
  /* fall through */
  case OC_IF_RW:
    oc_rep_set_boolean(root, state, state);
    oc_rep_set_int(root, power, power);
    oc_rep_set_text_string(root, name, oc_string(name));
    break;
  default:
    break;
  }
  oc_rep_end_root_object();
  oc_send_response(request, OC_STATUS_OK);
}

static void
post_light(oc_request_t *request, oc_interface_mask_t iface_mask,
           void *user_data)
{
  (void)iface_mask;
  (void)user_data;
  PRINT("POST_light:\n");
  oc_rep_t *rep = request->request_payload;
  while (rep != NULL) {
    PRINT("key: %s ", oc_string(rep->name));
    switch (rep->type) {
    case OC_REP_BOOL:
      state = rep->value.boolean;
      PRINT("value: %d\n", state);
      break;
    case OC_REP_INT:
      power = (int)rep->value.integer;
      PRINT("value: %d\n", power);
      break;
    case OC_REP_STRING:
      oc_free_string(&name);
      oc_new_string(&name, oc_string(rep->value.string),
                    oc_string_len(rep->value.string));
      break;
    default:
      oc_send_response(request, OC_STATUS_BAD_REQUEST);
      return;
      break;
    }
    rep = rep->next;
  }
  oc_send_response(request, OC_STATUS_CHANGED);
}

static void
put_light(oc_request_t *request, oc_interface_mask_t iface_mask,
          void *user_data)
{
  (void)iface_mask;
  (void)user_data;
  post_light(request, iface_mask, user_data);
}

static void
register_resources(void)
{
  oc_resource_t *res = oc_new_resource(NULL, "/a/light", 2, 0);
  oc_resource_bind_resource_type(res, "core.light");
  oc_resource_bind_resource_type(res, "core.brightlight");
  oc_resource_bind_resource_interface(res, OC_IF_RW);
  oc_resource_set_default_interface(res, OC_IF_RW);
  oc_resource_set_discoverable(res, true);
  oc_resource_set_periodic_observable(res, 1);
  oc_resource_set_request_handler(res, OC_GET, get_light, NULL);
  oc_resource_set_request_handler(res, OC_PUT, put_light, NULL);
  oc_resource_set_request_handler(res, OC_POST, post_light, NULL);
  oc_add_resource(res);
}

static void
signal_event_loop(void)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);
}

static void
handle_signal(int signal)
{
  (void)signal;
  signal_event_loop();
  quit = 1;
}

static void
factory_presets_cb(size_t device, void *data)
{
  (void)device;
  (void)data;
#if defined(OC_SECURITY) && defined(OC_PKI)
  const unsigned char my_crt[] = {
    0x30, 0x82, 0x03, 0xf8, 0x30, 0x82, 0x03, 0x9e, 0xa0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x09, 0x00, 0x8d, 0x0a, 0xfb, 0x7b, 0x53, 0xb2, 0x4c, 0xb6,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
    0x30, 0x5b, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c,
    0x03, 0x4f, 0x43, 0x46, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04,
    0x0b, 0x0c, 0x19, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x65, 0x73,
    0x74, 0x20, 0x49, 0x6e, 0x66, 0x72, 0x61, 0x73, 0x74, 0x72, 0x75, 0x63,
    0x74, 0x75, 0x72, 0x65, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x0c, 0x1e, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x45, 0x53,
    0x54, 0x20, 0x49, 0x6e, 0x74, 0x65, 0x72, 0x6d, 0x65, 0x64, 0x69, 0x61,
    0x74, 0x65, 0x20, 0x43, 0x41, 0x30, 0x30, 0x30, 0x32, 0x30, 0x1e, 0x17,
    0x0d, 0x31, 0x38, 0x31, 0x32, 0x31, 0x33, 0x31, 0x33, 0x33, 0x36, 0x33,
    0x30, 0x5a, 0x17, 0x0d, 0x31, 0x39, 0x30, 0x36, 0x31, 0x31, 0x31, 0x33,
    0x33, 0x36, 0x33, 0x30, 0x5a, 0x30, 0x61, 0x31, 0x0c, 0x30, 0x0a, 0x06,
    0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x4f, 0x43, 0x46, 0x31, 0x22, 0x30,
    0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c, 0x19, 0x4b, 0x79, 0x72, 0x69,
    0x6f, 0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6e, 0x66, 0x72, 0x61,
    0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x75, 0x72, 0x65, 0x31, 0x2d, 0x30,
    0x2b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x24, 0x39, 0x39, 0x30, 0x30,
    0x30, 0x30, 0x31, 0x30, 0x2d, 0x31, 0x31, 0x31, 0x31, 0x2d, 0x31, 0x31,
    0x31, 0x31, 0x2d, 0x31, 0x31, 0x31, 0x31, 0x2d, 0x31, 0x31, 0x31, 0x31,
    0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x30, 0x30, 0x59, 0x30, 0x13,
    0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
    0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xe4,
    0xbf, 0xcf, 0x9a, 0x29, 0x2d, 0x91, 0x54, 0x4b, 0x6d, 0x2c, 0x67, 0x38,
    0x7b, 0xe8, 0x7c, 0x58, 0x96, 0x60, 0x40, 0xc7, 0x72, 0xc2, 0x41, 0xb7,
    0x6f, 0xa6, 0x1a, 0x09, 0xe8, 0x85, 0x96, 0xad, 0x02, 0x4c, 0x95, 0xbf,
    0x67, 0x75, 0x24, 0x88, 0x98, 0x3c, 0x2a, 0x9a, 0xdb, 0x95, 0x96, 0x62,
    0x74, 0xce, 0x2d, 0x79, 0xe8, 0x30, 0xfa, 0x4c, 0x4a, 0x97, 0x5e, 0xaf,
    0xb9, 0xb3, 0x5c, 0xa3, 0x82, 0x02, 0x43, 0x30, 0x82, 0x02, 0x3f, 0x30,
    0x09, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x02, 0x30, 0x00, 0x30, 0x0e,
    0x06, 0x03, 0x55, 0x1d, 0x0f, 0x01, 0x01, 0xff, 0x04, 0x04, 0x03, 0x02,
    0x03, 0x88, 0x30, 0x29, 0x06, 0x03, 0x55, 0x1d, 0x25, 0x04, 0x22, 0x30,
    0x20, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x02, 0x06,
    0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06, 0x0a, 0x2b,
    0x06, 0x01, 0x04, 0x01, 0x82, 0xde, 0x7c, 0x01, 0x06, 0x30, 0x1d, 0x06,
    0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x46, 0x93, 0xfa, 0xa7,
    0x95, 0xbf, 0xbb, 0x0d, 0x1c, 0x38, 0xc4, 0xce, 0xe7, 0x49, 0x33, 0x16,
    0xe8, 0x59, 0xe9, 0xbe, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04,
    0x18, 0x30, 0x16, 0x80, 0x14, 0x19, 0x73, 0x6a, 0x04, 0x1a, 0x0b, 0x07,
    0x70, 0x4f, 0x53, 0x79, 0x53, 0x36, 0x87, 0xfc, 0x0c, 0xba, 0x7c, 0xae,
    0x0b, 0x30, 0x81, 0x96, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
    0x01, 0x01, 0x04, 0x81, 0x89, 0x30, 0x81, 0x86, 0x30, 0x5d, 0x06, 0x08,
    0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0x86, 0x51, 0x68, 0x74,
    0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x70, 0x6b, 0x69,
    0x2e, 0x6b, 0x79, 0x72, 0x69, 0x6f, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x6f,
    0x63, 0x66, 0x2f, 0x63, 0x61, 0x63, 0x65, 0x72, 0x74, 0x73, 0x2f, 0x42,
    0x42, 0x45, 0x36, 0x34, 0x46, 0x39, 0x41, 0x37, 0x45, 0x45, 0x33, 0x37,
    0x44, 0x32, 0x39, 0x41, 0x30, 0x35, 0x45, 0x34, 0x42, 0x42, 0x37, 0x37,
    0x35, 0x39, 0x35, 0x46, 0x33, 0x30, 0x38, 0x42, 0x45, 0x34, 0x31, 0x45,
    0x42, 0x30, 0x37, 0x2e, 0x63, 0x72, 0x74, 0x30, 0x25, 0x06, 0x08, 0x2b,
    0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x19, 0x68, 0x74, 0x74,
    0x70, 0x3a, 0x2f, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x6f, 0x63, 0x73, 0x70,
    0x2e, 0x6b, 0x79, 0x72, 0x69, 0x6f, 0x2e, 0x63, 0x6f, 0x6d, 0x30, 0x5f,
    0x06, 0x03, 0x55, 0x1d, 0x1f, 0x04, 0x58, 0x30, 0x56, 0x30, 0x54, 0xa0,
    0x52, 0xa0, 0x50, 0x86, 0x4e, 0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f,
    0x74, 0x65, 0x73, 0x74, 0x70, 0x6b, 0x69, 0x2e, 0x6b, 0x79, 0x72, 0x69,
    0x6f, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x6f, 0x63, 0x66, 0x2f, 0x63, 0x72,
    0x6c, 0x73, 0x2f, 0x42, 0x42, 0x45, 0x36, 0x34, 0x46, 0x39, 0x41, 0x37,
    0x45, 0x45, 0x33, 0x37, 0x44, 0x32, 0x39, 0x41, 0x30, 0x35, 0x45, 0x34,
    0x42, 0x42, 0x37, 0x37, 0x35, 0x39, 0x35, 0x46, 0x33, 0x30, 0x38, 0x42,
    0x45, 0x34, 0x31, 0x45, 0x42, 0x30, 0x37, 0x2e, 0x63, 0x72, 0x6c, 0x30,
    0x5f, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x83, 0x91, 0x56, 0x01,
    0x00, 0x04, 0x51, 0x30, 0x4f, 0x30, 0x09, 0x02, 0x01, 0x02, 0x02, 0x01,
    0x00, 0x02, 0x01, 0x00, 0x30, 0x36, 0x0c, 0x19, 0x31, 0x2e, 0x33, 0x2e,
    0x36, 0x2e, 0x31, 0x2e, 0x34, 0x2e, 0x31, 0x2e, 0x35, 0x31, 0x34, 0x31,
    0x34, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x31, 0x2e, 0x30, 0x0c, 0x19, 0x31,
    0x2e, 0x33, 0x2e, 0x36, 0x2e, 0x31, 0x2e, 0x34, 0x2e, 0x31, 0x2e, 0x35,
    0x31, 0x34, 0x31, 0x34, 0x2e, 0x30, 0x2e, 0x30, 0x2e, 0x32, 0x2e, 0x30,
    0x0c, 0x03, 0x43, 0x54, 0x54, 0x0c, 0x05, 0x49, 0x6e, 0x74, 0x65, 0x6c,
    0x30, 0x2a, 0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0x83, 0x91, 0x56,
    0x01, 0x01, 0x04, 0x1c, 0x30, 0x1a, 0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04,
    0x01, 0x83, 0x91, 0x56, 0x01, 0x01, 0x00, 0x06, 0x0b, 0x2b, 0x06, 0x01,
    0x04, 0x01, 0x83, 0x91, 0x56, 0x01, 0x01, 0x01, 0x30, 0x30, 0x06, 0x0a,
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x83, 0x91, 0x56, 0x01, 0x02, 0x04, 0x22,
    0x30, 0x20, 0x0c, 0x0e, 0x31, 0x2e, 0x33, 0x2e, 0x36, 0x2e, 0x31, 0x2e,
    0x34, 0x2e, 0x31, 0x2e, 0x37, 0x31, 0x0c, 0x09, 0x44, 0x69, 0x73, 0x63,
    0x6f, 0x76, 0x65, 0x72, 0x79, 0x0c, 0x03, 0x31, 0x2e, 0x30, 0x30, 0x0a,
    0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x48,
    0x00, 0x30, 0x45, 0x02, 0x20, 0x21, 0xac, 0x87, 0x42, 0x81, 0x04, 0x85,
    0x2f, 0x99, 0x38, 0xd1, 0xfb, 0x1f, 0x9c, 0x2e, 0xa7, 0x56, 0xca, 0x58,
    0xb5, 0x89, 0xd4, 0x02, 0x7f, 0x2f, 0x6a, 0x63, 0x91, 0x4e, 0xdf, 0xe8,
    0x5e, 0x02, 0x21, 0x00, 0xd7, 0x0c, 0xc3, 0x66, 0x90, 0xc2, 0x7f, 0xa7,
    0x27, 0x97, 0xc1, 0x0a, 0x24, 0x1b, 0xdc, 0xb8, 0xd4, 0x48, 0xc1, 0xb6,
    0x8f, 0xce, 0xaa, 0x82, 0x0f, 0xb0, 0x3a, 0xd7, 0x41, 0x06, 0x6e, 0x1d
  };

  unsigned char my_key[] = {
    0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x34, 0x23, 0xa2, 0xf0,
    0x44, 0x8a, 0xe4, 0x4c, 0x8b, 0x21, 0x7e, 0x4c, 0x0d, 0x68, 0x8a,
    0xdc, 0xea, 0x5e, 0xcf, 0xb8, 0x60, 0x0a, 0x97, 0xe3, 0x5a, 0x78,
    0x13, 0xfb, 0x12, 0x48, 0xad, 0x0d, 0xa0, 0x0a, 0x06, 0x08, 0x2a,
    0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42,
    0x00, 0x04, 0xe4, 0xbf, 0xcf, 0x9a, 0x29, 0x2d, 0x91, 0x54, 0x4b,
    0x6d, 0x2c, 0x67, 0x38, 0x7b, 0xe8, 0x7c, 0x58, 0x96, 0x60, 0x40,
    0xc7, 0x72, 0xc2, 0x41, 0xb7, 0x6f, 0xa6, 0x1a, 0x09, 0xe8, 0x85,
    0x96, 0xad, 0x02, 0x4c, 0x95, 0xbf, 0x67, 0x75, 0x24, 0x88, 0x98,
    0x3c, 0x2a, 0x9a, 0xdb, 0x95, 0x96, 0x62, 0x74, 0xce, 0x2d, 0x79,
    0xe8, 0x30, 0xfa, 0x4c, 0x4a, 0x97, 0x5e, 0xaf, 0xb9, 0xb3, 0x5c
  };

  unsigned char int_ca[] = {
    0x30, 0x82, 0x02, 0xfa, 0x30, 0x82, 0x02, 0xa1, 0xa0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x09, 0x00, 0xf3, 0x9b, 0x8c, 0xc0, 0x57, 0x2a, 0x11, 0xb5,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
    0x30, 0x53, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c,
    0x03, 0x4f, 0x43, 0x46, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04,
    0x0b, 0x0c, 0x19, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x65, 0x73,
    0x74, 0x20, 0x49, 0x6e, 0x66, 0x72, 0x61, 0x73, 0x74, 0x72, 0x75, 0x63,
    0x74, 0x75, 0x72, 0x65, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x0c, 0x16, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x45, 0x53,
    0x54, 0x20, 0x52, 0x4f, 0x4f, 0x54, 0x20, 0x43, 0x41, 0x30, 0x30, 0x30,
    0x32, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x38, 0x31, 0x31, 0x33, 0x30, 0x31,
    0x38, 0x31, 0x32, 0x31, 0x35, 0x5a, 0x17, 0x0d, 0x32, 0x38, 0x31, 0x31,
    0x32, 0x36, 0x31, 0x38, 0x31, 0x32, 0x31, 0x35, 0x5a, 0x30, 0x5b, 0x31,
    0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x4f, 0x43,
    0x46, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c, 0x19,
    0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49,
    0x6e, 0x66, 0x72, 0x61, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x75, 0x72,
    0x65, 0x31, 0x27, 0x30, 0x25, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x1e,
    0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x45, 0x53, 0x54, 0x20, 0x49,
    0x6e, 0x74, 0x65, 0x72, 0x6d, 0x65, 0x64, 0x69, 0x61, 0x74, 0x65, 0x20,
    0x43, 0x41, 0x30, 0x30, 0x30, 0x32, 0x30, 0x59, 0x30, 0x13, 0x06, 0x07,
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a, 0x86, 0x48,
    0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00, 0x04, 0xbc, 0x0f, 0x86,
    0x9f, 0x7a, 0x1f, 0x46, 0x91, 0xf8, 0xd1, 0x7b, 0x95, 0xa6, 0x90, 0x51,
    0x7f, 0xbf, 0x26, 0x0e, 0xd7, 0xdc, 0x94, 0xe9, 0x01, 0x77, 0xbf, 0xf7,
    0xdb, 0x24, 0x1c, 0x98, 0xad, 0x8b, 0x43, 0x4c, 0x26, 0xfe, 0xec, 0xa5,
    0xd9, 0xcc, 0x9e, 0x00, 0x13, 0xee, 0x37, 0xa3, 0x45, 0x71, 0x1f, 0x7e,
    0x2d, 0x89, 0x17, 0x67, 0x93, 0xf8, 0x3a, 0xfc, 0xbd, 0x47, 0x8d, 0xd0,
    0xbe, 0xa3, 0x82, 0x01, 0x54, 0x30, 0x82, 0x01, 0x50, 0x30, 0x12, 0x06,
    0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01,
    0x01, 0xff, 0x02, 0x01, 0x00, 0x30, 0x0e, 0x06, 0x03, 0x55, 0x1d, 0x0f,
    0x01, 0x01, 0xff, 0x04, 0x04, 0x03, 0x02, 0x01, 0x86, 0x30, 0x1d, 0x06,
    0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x19, 0x73, 0x6a, 0x04,
    0x1a, 0x0b, 0x07, 0x70, 0x4f, 0x53, 0x79, 0x53, 0x36, 0x87, 0xfc, 0x0c,
    0xba, 0x7c, 0xae, 0x0b, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04,
    0x18, 0x30, 0x16, 0x80, 0x14, 0x28, 0x48, 0xe4, 0xe5, 0x27, 0x58, 0xd9,
    0x08, 0xee, 0x09, 0x34, 0xe4, 0xb1, 0xbb, 0x3d, 0x59, 0x66, 0x1f, 0xc8,
    0xf5, 0x30, 0x81, 0x8d, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07,
    0x01, 0x01, 0x04, 0x81, 0x80, 0x30, 0x7e, 0x30, 0x55, 0x06, 0x08, 0x2b,
    0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x02, 0x86, 0x49, 0x68, 0x74, 0x74,
    0x70, 0x3a, 0x2f, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x70, 0x6b, 0x69, 0x2e,
    0x6b, 0x79, 0x72, 0x69, 0x6f, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x6f, 0x63,
    0x66, 0x2f, 0x34, 0x45, 0x36, 0x38, 0x45, 0x33, 0x46, 0x43, 0x46, 0x30,
    0x46, 0x32, 0x45, 0x34, 0x46, 0x38, 0x30, 0x41, 0x38, 0x44, 0x31, 0x34,
    0x33, 0x38, 0x46, 0x36, 0x41, 0x31, 0x42, 0x41, 0x35, 0x36, 0x39, 0x35,
    0x37, 0x31, 0x33, 0x44, 0x36, 0x33, 0x2e, 0x63, 0x72, 0x74, 0x30, 0x25,
    0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x86, 0x19,
    0x68, 0x74, 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x6f,
    0x63, 0x73, 0x70, 0x2e, 0x6b, 0x79, 0x72, 0x69, 0x6f, 0x2e, 0x63, 0x6f,
    0x6d, 0x30, 0x5a, 0x06, 0x03, 0x55, 0x1d, 0x1f, 0x04, 0x53, 0x30, 0x51,
    0x30, 0x4f, 0xa0, 0x4d, 0xa0, 0x4b, 0x86, 0x49, 0x68, 0x74, 0x74, 0x70,
    0x3a, 0x2f, 0x2f, 0x74, 0x65, 0x73, 0x74, 0x70, 0x6b, 0x69, 0x2e, 0x6b,
    0x79, 0x72, 0x69, 0x6f, 0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x6f, 0x63, 0x66,
    0x2f, 0x34, 0x45, 0x36, 0x38, 0x45, 0x33, 0x46, 0x43, 0x46, 0x30, 0x46,
    0x32, 0x45, 0x34, 0x46, 0x38, 0x30, 0x41, 0x38, 0x44, 0x31, 0x34, 0x33,
    0x38, 0x46, 0x36, 0x41, 0x31, 0x42, 0x41, 0x35, 0x36, 0x39, 0x35, 0x37,
    0x31, 0x33, 0x44, 0x36, 0x33, 0x2e, 0x63, 0x72, 0x6c, 0x30, 0x0a, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, 0x03, 0x47, 0x00,
    0x30, 0x44, 0x02, 0x1f, 0x05, 0xe4, 0x45, 0x87, 0x7e, 0xbb, 0x9a, 0x4e,
    0x3c, 0x7e, 0x78, 0xe3, 0x00, 0x66, 0x05, 0x12, 0x73, 0xfd, 0xbd, 0x23,
    0xa6, 0x9b, 0xd4, 0x20, 0x7c, 0x7c, 0x21, 0x41, 0xf4, 0x0a, 0x2a, 0x02,
    0x21, 0x00, 0xc2, 0xf0, 0x29, 0xcc, 0x55, 0x33, 0x82, 0xe5, 0xa2, 0x28,
    0xa3, 0x96, 0x20, 0xe2, 0x4e, 0xc1, 0x0c, 0x33, 0x71, 0x6d, 0x14, 0x28,
    0x3e, 0xe8, 0xd8, 0x7a, 0xcd, 0x0e, 0x4d, 0x51, 0xa0, 0x3c
  };

  unsigned char root_ca[] = {
    0x30, 0x82, 0x01, 0xdf, 0x30, 0x82, 0x01, 0x85, 0xa0, 0x03, 0x02, 0x01,
    0x02, 0x02, 0x09, 0x00, 0xf3, 0x9b, 0x8c, 0xc0, 0x57, 0x2a, 0x11, 0xb2,
    0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
    0x30, 0x53, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c,
    0x03, 0x4f, 0x43, 0x46, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04,
    0x0b, 0x0c, 0x19, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x65, 0x73,
    0x74, 0x20, 0x49, 0x6e, 0x66, 0x72, 0x61, 0x73, 0x74, 0x72, 0x75, 0x63,
    0x74, 0x75, 0x72, 0x65, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x0c, 0x16, 0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x45, 0x53,
    0x54, 0x20, 0x52, 0x4f, 0x4f, 0x54, 0x20, 0x43, 0x41, 0x30, 0x30, 0x30,
    0x32, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x38, 0x31, 0x31, 0x33, 0x30, 0x31,
    0x37, 0x33, 0x31, 0x30, 0x35, 0x5a, 0x17, 0x0d, 0x32, 0x38, 0x31, 0x31,
    0x32, 0x37, 0x31, 0x37, 0x33, 0x31, 0x30, 0x35, 0x5a, 0x30, 0x53, 0x31,
    0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x4f, 0x43,
    0x46, 0x31, 0x22, 0x30, 0x20, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x0c, 0x19,
    0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49,
    0x6e, 0x66, 0x72, 0x61, 0x73, 0x74, 0x72, 0x75, 0x63, 0x74, 0x75, 0x72,
    0x65, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x16,
    0x4b, 0x79, 0x72, 0x69, 0x6f, 0x20, 0x54, 0x45, 0x53, 0x54, 0x20, 0x52,
    0x4f, 0x4f, 0x54, 0x20, 0x43, 0x41, 0x30, 0x30, 0x30, 0x32, 0x30, 0x59,
    0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01, 0x06,
    0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03, 0x42, 0x00,
    0x04, 0x6b, 0x75, 0xb1, 0x4d, 0x90, 0x85, 0x07, 0x0a, 0xfe, 0x47, 0xe5,
    0x29, 0x21, 0x7d, 0x4c, 0x2a, 0xef, 0x29, 0xa0, 0xdc, 0x90, 0xb5, 0x9d,
    0x66, 0x8c, 0xaf, 0x3f, 0xac, 0xf4, 0x3a, 0xba, 0x8d, 0x76, 0xd0, 0x6c,
    0x71, 0x98, 0x15, 0x62, 0xc4, 0x87, 0x31, 0x06, 0x75, 0x47, 0x5f, 0x70,
    0x5b, 0x1b, 0x1f, 0x96, 0xf3, 0x6b, 0xf1, 0xb3, 0x15, 0x5b, 0x52, 0xb7,
    0x1d, 0x63, 0x24, 0xa6, 0xc8, 0xa3, 0x42, 0x30, 0x40, 0x30, 0x0f, 0x06,
    0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x05, 0x30, 0x03, 0x01,
    0x01, 0xff, 0x30, 0x0e, 0x06, 0x03, 0x55, 0x1d, 0x0f, 0x01, 0x01, 0xff,
    0x04, 0x04, 0x03, 0x02, 0x01, 0x86, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d,
    0x0e, 0x04, 0x16, 0x04, 0x14, 0x28, 0x48, 0xe4, 0xe5, 0x27, 0x58, 0xd9,
    0x08, 0xee, 0x09, 0x34, 0xe4, 0xb1, 0xbb, 0x3d, 0x59, 0x66, 0x1f, 0xc8,
    0xf5, 0x30, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03,
    0x02, 0x03, 0x48, 0x00, 0x30, 0x45, 0x02, 0x20, 0x25, 0x31, 0x4c, 0x20,
    0x55, 0xe2, 0xfc, 0x77, 0x95, 0xb8, 0x8d, 0x97, 0x45, 0x27, 0x96, 0x60,
    0x72, 0x59, 0x3b, 0x5d, 0x3e, 0xba, 0x2c, 0xd3, 0x1f, 0x1a, 0x41, 0x31,
    0x4a, 0x35, 0x35, 0x9e, 0x02, 0x21, 0x00, 0xd3, 0xaf, 0x4e, 0x67, 0x77,
    0xd8, 0x0d, 0x24, 0x12, 0xd2, 0x29, 0x1d, 0xb8, 0x8a, 0x03, 0xcf, 0x91,
    0x14, 0x30, 0x8f, 0x25, 0x68, 0xcd, 0xe2, 0x5a, 0x31, 0xac, 0x10, 0xbb,
    0xbf, 0x42, 0x44
  };

  int credid =
    oc_pki_add_mfg_cert(0, my_crt, sizeof(my_crt), my_key, sizeof(my_key));

  oc_pki_add_mfg_intermediate_cert(0, credid, int_ca, sizeof(int_ca));

  oc_pki_add_mfg_trust_anchor(0, root_ca, sizeof(root_ca));

  oc_pki_set_security_profile(0, OC_SP_BLACK, OC_SP_BLACK, credid);
#endif /* OC_SECURITY && OC_PKI */
}

int
main(void)
{
  int init;
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);

  static const oc_handler_t handler = { .init = app_init,
                                        .signal_event_loop = signal_event_loop,
                                        .register_resources =
                                          register_resources };

  oc_clock_time_t next_event;

  oc_set_con_res_announced(false);
  oc_set_mtu_size(16384);
  oc_set_max_app_data_size(16384);

#ifdef OC_STORAGE
  oc_storage_config("./simpleserver_pki_creds");
#endif /* OC_STORAGE */

  oc_set_factory_presets_cb(factory_presets_cb, NULL);

  init = oc_main_init(&handler);
  if (init < 0) {
    return init;
  }

  while (quit != 1) {
    next_event = oc_main_poll();
    pthread_mutex_lock(&mutex);
    if (next_event == 0) {
      pthread_cond_wait(&cv, &mutex);
    } else {
      ts.tv_sec = (next_event / OC_CLOCK_SECOND);
      ts.tv_nsec = (next_event % OC_CLOCK_SECOND) * 1.e09 / OC_CLOCK_SECOND;
      pthread_cond_timedwait(&cv, &mutex, &ts);
    }
    pthread_mutex_unlock(&mutex);
  }
  oc_free_string(&name);
  oc_main_shutdown();
  return 0;
}
