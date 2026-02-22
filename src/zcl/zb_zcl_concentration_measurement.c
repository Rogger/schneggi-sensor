/*
 * ZBOSS Zigbee 3.0
 *
 * Copyright (c) 2012-2020 DSR Corporation, Denver CO, USA.
 * www.dsr-zboss.com
 * www.dsr-corporation.com
 * All rights reserved.
 *
 *
 * Use in source and binary forms, redistribution in binary form only, with
 * or without modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 2. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 3. This software, with or without modification, must only be used with a Nordic
 *    Semiconductor ASA integrated circuit.
 *
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* PURPOSE:
*/

#define ZB_TRACE_FILE_ID 12086

#include "zb_zcl_concentration_measurement.h"

zb_ret_t check_value_concentration_measurement_server(zb_uint16_t attr_id, zb_uint8_t endpoint, zb_uint8_t *value);

static zb_bool_t zcl_single_is_nan(zb_uint32_t raw)
{
  return ((raw & 0x7F800000u) == 0x7F800000u) && ((raw & 0x007FFFFFu) != 0u);
}

static float zcl_single_to_float(zb_uint32_t raw)
{
  union
  {
    zb_uint32_t raw_value;
    float float_value;
  } data = {.raw_value = raw};

  return data.float_value;
}

void zb_zcl_concentration_init_server()
{
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
                              ZB_ZCL_CLUSTER_SERVER_ROLE,
                              check_value_concentration_measurement_server,
                              (zb_zcl_cluster_write_attr_hook_t)NULL,
                              (zb_zcl_cluster_handler_t)NULL);
}

void zb_zcl_concentration_init_client()
{
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
                              ZB_ZCL_CLUSTER_CLIENT_ROLE,
                              (zb_zcl_cluster_check_value_t)NULL,
                              (zb_zcl_cluster_write_attr_hook_t)NULL,
                              (zb_zcl_cluster_handler_t)NULL);
}

zb_ret_t check_value_concentration_measurement_server(zb_uint16_t attr_id, zb_uint8_t endpoint, zb_uint8_t *value)
{
  zb_ret_t ret = RET_OK;

  TRACE_MSG(TRACE_ZCL1, "> check_value_concentration_measurement", (FMT__0));

  switch( attr_id )
  {
    case ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_VALUE_ID:
      {
        zb_uint32_t measured_raw = ZB_ZCL_ATTR_GET32(value);
        float measured_value = zcl_single_to_float(measured_raw);

        if (zcl_single_is_nan(measured_raw))
        {
          ret = RET_OK;
          break;
        }

        if (measured_value < 0.0f || measured_value > 1.0f)
        {
          ret = RET_ERROR;
          break;
        }

        zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
            endpoint,
            ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
            ZB_ZCL_CLUSTER_SERVER_ROLE,
            ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MIN_VALUE_ID);
        zb_uint32_t min_raw;

        ZB_ASSERT(attr_desc);
        min_raw = ZB_ZCL_GET_ATTRIBUTE_VAL_32(attr_desc);

        if (!zcl_single_is_nan(min_raw) && measured_value < zcl_single_to_float(min_raw))
        {
          ret = RET_ERROR;
          break;
        }

        attr_desc = zb_zcl_get_attr_desc_a(
            endpoint,
            ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
            ZB_ZCL_CLUSTER_SERVER_ROLE,
            ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MAX_VALUE_ID);
        ZB_ASSERT(attr_desc);

        {
          zb_uint32_t max_raw = ZB_ZCL_GET_ATTRIBUTE_VAL_32(attr_desc);

          ret = (!zcl_single_is_nan(max_raw) && measured_value > zcl_single_to_float(max_raw))
            ? RET_ERROR
            : RET_OK;
        }
      }
      break;

    case ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MIN_VALUE_ID:
      {
        zb_uint32_t min_raw = ZB_ZCL_ATTR_GET32(value);
        float min_value = zcl_single_to_float(min_raw);

        if (zcl_single_is_nan(min_raw))
        {
          ret = RET_OK;
          break;
        }

        if (min_value < 0.0f || min_value >= 1.0f)
        {
          ret = RET_ERROR;
          break;
        }

        {
          zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
              endpoint,
              ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
              ZB_ZCL_CLUSTER_SERVER_ROLE,
              ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MAX_VALUE_ID);
          zb_uint32_t max_raw;

          ZB_ASSERT(attr_desc);
          max_raw = ZB_ZCL_GET_ATTRIBUTE_VAL_32(attr_desc);

          ret = (!zcl_single_is_nan(max_raw) && !(min_value < zcl_single_to_float(max_raw)))
            ? RET_ERROR
            : RET_OK;
        }
      }
      break;

    case ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MAX_VALUE_ID:
      {
        zb_uint32_t max_raw = ZB_ZCL_ATTR_GET32(value);
        float max_value = zcl_single_to_float(max_raw);

        if (zcl_single_is_nan(max_raw))
        {
          ret = RET_OK;
          break;
        }

        if (max_value <= 0.0f || max_value > 1.0f)
        {
          ret = RET_ERROR;
          break;
        }

        {
          zb_zcl_attr_t *attr_desc = zb_zcl_get_attr_desc_a(
              endpoint,
              ZB_ZCL_CLUSTER_ID_CONCENTRATION_MEASUREMENT,
              ZB_ZCL_CLUSTER_SERVER_ROLE,
              ZB_ZCL_ATTR_CONCENTRATION_MEASUREMENT_MIN_VALUE_ID);
          zb_uint32_t min_raw;

          ZB_ASSERT(attr_desc);
          min_raw = ZB_ZCL_GET_ATTRIBUTE_VAL_32(attr_desc);

          ret = (!zcl_single_is_nan(min_raw) && !(zcl_single_to_float(min_raw) < max_value))
            ? RET_ERROR
            : RET_OK;
        }
      }
      break;

    default:
      break;
  }

  TRACE_MSG(TRACE_ZCL1, "< check_value_concentration_measurement ret %hd", (FMT__H, ret));
  return ret;
}
