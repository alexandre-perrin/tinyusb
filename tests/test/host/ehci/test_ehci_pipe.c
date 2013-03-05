/*
 * test_ehci_pipe.c
 *
 *  Created on: Feb 27, 2013
 *      Author: hathach
 */

/*
 * Software License Agreement (BSD License)
 * Copyright (c) 2012, hathach (tinyusb.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the tiny usb stack.
 */

#include "unity.h"
#include "tusb_option.h"
#include "errors.h"
#include "binary.h"

#include "hal.h"
#include "mock_osal.h"
#include "hcd.h"
#include "usbh_hcd.h"
#include "ehci.h"

extern ehci_data_t ehci_data;
usbh_device_info_t usbh_device_info_pool[TUSB_CFG_HOST_DEVICE_MAX+1];

LPC_USB0_Type lpc_usb0;
LPC_USB1_Type lpc_usb1;

uint8_t const max_packet_size = 64;
uint8_t dev_addr = 0;
uint8_t hub_addr;
uint8_t hub_port;

//--------------------------------------------------------------------+
// Setup/Teardown + helper declare
//--------------------------------------------------------------------+
void setUp(void)
{
  memclr_(&lpc_usb0, sizeof(LPC_USB0_Type));
  memclr_(&lpc_usb1, sizeof(LPC_USB1_Type));

  memclr_(usbh_device_info_pool, sizeof(usbh_device_info_t)*(TUSB_CFG_HOST_DEVICE_MAX+1));

  dev_addr = 0;
  hub_addr = 2;
  hub_port = 2;
}

void tearDown(void)
{
}

//--------------------------------------------------------------------+
// CONTROL PIPE
//--------------------------------------------------------------------+
void verify_control_open_qhd(ehci_qhd_t *p_qhd)
{
  TEST_ASSERT_EQUAL(dev_addr, p_qhd->device_address);
  TEST_ASSERT_FALSE(p_qhd->inactive_next_xact);
  TEST_ASSERT_EQUAL(0, p_qhd->endpoint_number);
  TEST_ASSERT_EQUAL(1, p_qhd->data_toggle_control);
  TEST_ASSERT_EQUAL(max_packet_size, p_qhd->max_package_size);
  TEST_ASSERT_EQUAL(0, p_qhd->nak_count_reload); // TODO NAK Reload disable

  TEST_ASSERT_EQUAL(0, p_qhd->smask);
  TEST_ASSERT_EQUAL(0, p_qhd->cmask);
  TEST_ASSERT_EQUAL(hub_addr, p_qhd->hub_address);
  TEST_ASSERT_EQUAL(hub_port, p_qhd->hub_port);
  TEST_ASSERT_EQUAL(1, p_qhd->mult);

  TEST_ASSERT(p_qhd->qtd_overlay.next.terminate);
  TEST_ASSERT(p_qhd->qtd_overlay.alternate.terminate);
  TEST_ASSERT(p_qhd->qtd_overlay.halted);

  //------------- HCD -------------//
  TEST_ASSERT(p_qhd->used);
  TEST_ASSERT_NULL(p_qhd->p_qtd_list);
}

void test_control_open_addr0_qhd_data(void)
{
  dev_addr = 0;
  for (uint8_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid = i + TEST_CONTROLLER_HOST_START_INDEX;
    ehci_qhd_t * const p_qhd = get_async_head( hostid );

    usbh_device_info_pool[dev_addr].core_id = hostid;
    usbh_device_info_pool[dev_addr].hub_addr = hub_addr;
    usbh_device_info_pool[dev_addr].hub_port = hub_port;

    hcd_pipe_control_open(dev_addr, max_packet_size);

    verify_control_open_qhd(p_qhd);
    TEST_ASSERT(p_qhd->head_list_flag);
  }
}

void test_control_open_qhd_data(void)
{
  dev_addr = 1;
  for (uint8_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid = i + TEST_CONTROLLER_HOST_START_INDEX;
    ehci_qhd_t * const async_head =  get_async_head( hostid );
    ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;


    usbh_device_info_pool[dev_addr].core_id = hostid;
    usbh_device_info_pool[dev_addr].hub_addr = hub_addr;
    usbh_device_info_pool[dev_addr].hub_port = hub_port;

    hcd_pipe_control_open(dev_addr, max_packet_size);

    verify_control_open_qhd(p_qhd);
    TEST_ASSERT_FALSE(p_qhd->head_list_flag);

    //------------- async list check -------------//
    TEST_ASSERT_EQUAL_HEX((uint32_t) p_qhd, align32(async_head->next.address));
    TEST_ASSERT_FALSE(async_head->next.terminate);
    TEST_ASSERT_EQUAL(EHCI_QUEUE_ELEMENT_QHD, async_head->next.type);
  }
}

void test_control_open_highspeed(void)
{
  dev_addr = 1;
  for (uint8_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid = i + TEST_CONTROLLER_HOST_START_INDEX;
    ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;

    usbh_device_info_pool[dev_addr].core_id = hostid;
    usbh_device_info_pool[dev_addr].speed = TUSB_SPEED_HIGH;

    hcd_pipe_control_open(dev_addr, max_packet_size);

    TEST_ASSERT_EQUAL(TUSB_SPEED_HIGH, p_qhd->endpoint_speed);
    TEST_ASSERT_FALSE(p_qhd->non_hs_control_endpoint);
  }
}

void test_control_open_non_highspeed(void)
{
  dev_addr = 1;
  for (uint8_t i=0; i<CONTROLLER_HOST_NUMBER; i++)
  {
    uint8_t hostid = i + TEST_CONTROLLER_HOST_START_INDEX;
    ehci_qhd_t * const p_qhd = &ehci_data.device[dev_addr].control.qhd;

    usbh_device_info_pool[dev_addr].core_id = hostid;
    usbh_device_info_pool[dev_addr].speed = TUSB_SPEED_FULL;

    hcd_pipe_control_open(dev_addr, max_packet_size);

    TEST_ASSERT_EQUAL(TUSB_SPEED_FULL, p_qhd->endpoint_speed);
    TEST_ASSERT_TRUE(p_qhd->non_hs_control_endpoint);
  }
}

void test_control_open_device_not_connected(void)
{

}
