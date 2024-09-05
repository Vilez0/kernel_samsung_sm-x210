/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for ic core functions
 *
 *  Copyright (C) 2022 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_ic_core.h"
//+P86801AA1, peiyuexiang.wt, 20230703, add, hardware_info
//#include <linux/hardware_info.h>
extern char TP_name[HARDWARE_MAX_ITEM_LONGTH];
//-P86801AA1, peiyuexiang.wt, 20230703, add, hardware_info

#if defined(HX_FIRMWARE_HEADER)
extern struct firmware g_embedded_fw;
#endif

bool g_has_alg_overlay;
#if defined(HX_ZERO_FLASH)

uint8_t *ovl_idx;

#endif

int HX_TOUCH_INFO_POINT_CNT;

void (*himax_mcu_cmd_struct_free)(void);
// uint8_t *g_internal_buffer;
uint32_t dbg_reg_ary[4] = {ADDR_FW_DBG_MSG,
	ADDR_CHK_FW_STATUS,
	ADDR_CHK_DD_STATUS,
	ADDR_FLAG_RESET_EVENT};

/* CORE_IC */
/* IC side start*/

void himax_mcu_leave_safe_mode(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	hx_parse_assign_cmd(hx_s_ic_setup._data_leave_safe_mode,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_leave_safe_mode,
		tmp_data, DATA_LEN_4);
}

static int himax_write_read_reg(uint32_t tmp_addr, uint8_t *tmp_data,
		uint8_t hb, uint8_t lb)
{
	uint16_t retry = 0;
	uint8_t r_data[DATA_LEN_4] = {0};

	while (retry++ < 40) { /* ceil[16.6*2] */
		hx_s_core_fp._register_read(tmp_addr, r_data, DATA_LEN_4);
		if (r_data[1] == lb && r_data[0] == hb)
			break;
		else if (r_data[1] == hb && r_data[0] == lb)
			return NO_ERR;

		hx_s_core_fp._register_write(tmp_addr, tmp_data, DATA_LEN_4);
		usleep_range(1000, 1100);
	}

	if (retry >= 40)
		goto FAIL;

	retry = 0;
	while (retry++ < 200) { /* self test item might take long time */
		hx_s_core_fp._register_read(tmp_addr, r_data, DATA_LEN_4);
		if (r_data[1] == hb && r_data[0] == lb)
			return NO_ERR;

		I("%s: wait data ready %d times\n", __func__, retry);
		usleep_range(10000, 10100);
	}

FAIL:
	E("%s: failed to handshaking with DSRAM\n", __func__);
	E("%s: addr = %08X; data = %02X%02X%02X%02X",
		__func__, tmp_addr,
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
	E("%s: target = %02X%02X; r_data = %02X%02X\n",
		__func__, hb, lb, r_data[1], r_data[0]);

	return HX_RW_REG_FAIL;

}

void himax_mcu_interface_on(void)
{
	uint8_t tmp_addr[DATA_LEN_4];
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_data2[DATA_LEN_4];
	int cnt = 0;
	int ret = 0;

	/* Read a dummy register to wake up BUS.*/
	ret = himax_bus_read(ADDR_AHB_RDATA_BYTE_0,
		tmp_data, DATA_LEN_4);
	if (ret < 0) {/* to knock BUS*/
		E("%s: bus access fail!\n", __func__);
		return;
	}

	do {
		hx_parse_assign_cmd(ADDR_AHB_CONTINOUS, tmp_addr, DATA_LEN_1);
		hx_parse_assign_cmd(PARA_AHB_CONTINOUS, tmp_data, DATA_LEN_1);
		ret = himax_bus_write(tmp_addr[0], NUM_NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return;
		}

		hx_parse_assign_cmd(ADDR_AHB_INC4, tmp_addr, DATA_LEN_1);
		hx_parse_assign_cmd(PARA_AHB_INC4, tmp_data, DATA_LEN_1);
		ret = himax_bus_write(tmp_addr[0], NUM_NULL, tmp_data, 1);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return;
		}

		/*Check cmd*/
		himax_bus_read(ADDR_AHB_CONTINOUS, tmp_data, 1);

		himax_bus_read(ADDR_AHB_INC4, tmp_data2, 1);

		if (tmp_data[0] == PARA_AHB_CONTINOUS
		&& tmp_data2[0] == PARA_AHB_INC4)
			break;

		usleep_range(1000, 1100);
	} while (++cnt < 10);

	if (cnt > 0)
		I("%s:Polling burst mode: %d times\n", __func__, cnt);
}

#define WIP_PRT_LOG "%s: retry:%d, bf[0]=%d, bf[1]=%d,bf[2]=%d, bf[3]=%d\n"
bool himax_mcu_wait_wip(int Timing)
{
	uint8_t tmp_data[DATA_LEN_4];
	int retry_cnt = 0;


	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);
	tmp_data[0] = 0x01;

	do {
		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_1,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_1,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);

		tmp_data[0] = tmp_data[1] = tmp_data[2] = tmp_data[3] = 0xFF;
		hx_s_core_fp._register_read(
			hx_s_ic_setup._addr_flash_spi200_data,
			tmp_data, 4);

		if ((tmp_data[0] & 0x01) == 0x00)
			return true;

		retry_cnt++;

		if (tmp_data[0] != 0x00
		|| tmp_data[1] != 0x00
		|| tmp_data[2] != 0x00
		|| tmp_data[3] != 0x00)
			I(WIP_PRT_LOG,
			__func__, retry_cnt, tmp_data[0],
			tmp_data[1], tmp_data[2], tmp_data[3]);

		if (retry_cnt > 100) {
			E("%s: Wait wip error!\n", __func__);
			return false;
		}

		msleep(Timing);
	} while ((tmp_data[0] & 0x01) == 0x01);

	return true;
}


/*power saving level*/
void himax_mcu_init_psl(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_psl, tmp_data,
		sizeof(tmp_data));
	I("%s: power saving level reset OK!\n", __func__);
}

void himax_mcu_resume_ic_action(void)
{
	/* Nothing to do */
}

void himax_mcu_suspend_ic_action(void)
{
	/* Nothing to do */
}

void himax_mcu_power_on_init(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0x01, 0x00, 0x00, 0x00};
	uint8_t retry = 0;

	/*RawOut select initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawout_sel,
		tmp_data, DATA_LEN_4);
	/*DSRAM func initial*/
	hx_s_core_fp._assign_sorting_mode(tmp_data);
	/*N frame initial*/
	/* reset N frame back to default value 1 for normal mode */
	hx_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_set_frame,
		tmp_data, 4);
	/*FW reload done initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_chk_fw_reload2,
		tmp_data, DATA_LEN_4);
#if defined(SEC_PALM_FUNC)
	hx_parse_assign_cmd(hx_s_ic_setup._data_hand_blade_on_off,
		tmp_data, DATA_LEN_4);	
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_hand_blade,
			tmp_data, DATA_LEN_4);
#endif
	hx_s_core_fp._sense_on(0x00);

	I("%s: waiting for FW reload data\n", __func__);

	while (retry++ < 30) {
		// hx_parse_assign_cmd(hx_s_ic_setup._addr_chk_fw_reload2,
		// tmp_addr, DATA_LEN_4);
		// hx_s_core_fp._register_read(tmp_addr, tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_chk_fw_reload2,
			tmp_data, DATA_LEN_4);
		/* use all 4 bytes to compare */
		if ((tmp_data[3] == 0x00 && tmp_data[2] == 0x00 &&
			tmp_data[1] == 0x72 && tmp_data[0] == 0xC0)) {
			I("%s: FW reload done\n", __func__);
			break;
		}
		I("%s: wait FW reload %d times\n", __func__, retry);
		hx_s_core_fp._read_FW_status();
		usleep_range(10000, 11000);
	}
}
/* IC side end*/
/* CORE_IC */

/* CORE_FW */
/* FW side start*/
static int diag_mcu_parse_raw_data(struct himax_report_data *hx_s_touch_data,
		int mul_num, int self_num, uint8_t diag_cmd,
		int32_t *mutual_data, int32_t *self_data)
{
	int RawDataLen_word;
	int temp1 = 0;
	int temp2 = 0;
	int i = 0;
	int index = 0;
	uint32_t checksum = 0;
	int res = NO_ERR;

	if (!hx_s_debug_data->is_stack_full_raw) {
		if (hx_s_touch_data->rawdata_buf[0]
		== hx_s_ic_setup._data_rawdata_ready_lb
		&& hx_s_touch_data->rawdata_buf[1]
		== hx_s_ic_setup._data_rawdata_ready_hb
		&& hx_s_touch_data->rawdata_buf[2] > 0
		&& hx_s_touch_data->rawdata_buf[3] == diag_cmd) {
			RawDataLen_word = hx_s_touch_data->rawdata_size / 2;
			index = (hx_s_touch_data->rawdata_buf[2] - 1)
				* RawDataLen_word;

			for (i = 0; i < RawDataLen_word; i++) {
				temp1 = index + i;

				if (temp1 < mul_num) { /*mutual*/
					mutual_data[index + i] =
						((int8_t)hx_s_touch_data->
						rawdata_buf[i * 2 + 4 + 1]) *
							256
						+ hx_s_touch_data->
						rawdata_buf[i * 2 + 4];
				} else { /*self*/
					temp1 = i + index;
					temp2 = self_num + mul_num;

					if (temp1 >= temp2)
						break;

					self_data[i + index - mul_num] =
						(((int8_t)hx_s_touch_data->
						rawdata_buf[i * 2 + 4 + 1]) <<
							8)
						+ hx_s_touch_data->
						rawdata_buf[i * 2 + 4];
				}
			}
		}
	} else {
		checksum += (((int8_t)hx_s_touch_data->
					rawdata_buf[1]) * 256
					+ hx_s_touch_data->
					rawdata_buf[0]);
		checksum += (((int8_t)hx_s_touch_data->
					rawdata_buf[3]) * 256
					+ hx_s_touch_data->
					rawdata_buf[2]);
		RawDataLen_word = hx_s_touch_data->rawdata_size / 2;
		for (i = 0; i < RawDataLen_word; i++) {
			if (temp1 < mul_num) { /*mutual*/
				mutual_data[i] =
					((int8_t)hx_s_touch_data->
					rawdata_buf[i * 2 + 4 + 1]) * 256
					+ hx_s_touch_data->
					rawdata_buf[i * 2 + 4];
			} else { /*self*/
				temp1 = i;
				temp2 = self_num + mul_num;

				if (temp1 >= temp2)
					break;

				self_data[i - mul_num] =
					(((int8_t)hx_s_touch_data->
					rawdata_buf[i * 2 + 4 + 1]) << 8)
					+ hx_s_touch_data->
					rawdata_buf[i * 2 + 4];
			}
			checksum += (((int8_t)hx_s_touch_data->
					rawdata_buf[i * 2 + 4 + 1]) * 256
					+ hx_s_touch_data->
					rawdata_buf[i * 2 + 4]);
		}

		if ((checksum % 0x10000) != 0) {
			I("%s: Now checksum=0x%08X\n", __func__, checksum);
			I("%s: Checksum fail!\n", __func__);
			res = CHECKSUM_FAIL;
		}
	}
	return res;
}

int himax_mcu_calc_crc_by_ap(const uint8_t *fw_content,
		int crc_from_fw, int len)
{
	int i, j, length = 0;
	int fw_data;
	int fw_data_2;
	int crc = 0xFFFFFFFF;
	int poly_nomial = 0x82F63B78;

	length = len / 4;

	for (i = 0; i < length; i++) {
		fw_data = fw_content[i * 4];

		for (j = 1; j < 4; j++) {
			fw_data_2 = fw_content[i * 4 + j];
			fw_data += (fw_data_2) << (8 * j);
		}
		crc = fw_data ^ crc;
		for (j = 0; j < 32; j++) {
			if ((crc % 2) != 0)
				crc = ((crc >> 1) & 0x7FFFFFFF) ^ poly_nomial;
			else
				crc = (((crc >> 1) & 0x7FFFFFFF));
		}
	}

	return crc;
}

uint32_t himax_mcu_calc_crc_by_hw(uint8_t *start_addr, int reload_length)
{
	uint32_t result = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int cnt = 0, ret = 0;
	int length = reload_length / DATA_LEN_4;

	ret = hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_reload_addr_from,
		start_addr, DATA_LEN_4);
	if (ret < NO_ERR) {
		E("%s: bus access fail!\n", __func__);
		return HW_CRC_FAIL;
	}

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x99;
	tmp_data[1] = (length >> 8);
	tmp_data[0] = length;
	ret = hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_reload_addr_cmd_beat,
		tmp_data, DATA_LEN_4);
	if (ret < NO_ERR) {
		E("%s: bus access fail!\n", __func__);
		return HW_CRC_FAIL;
	}
	cnt = 0;

	do {
		ret = hx_s_core_fp._register_read(
			hx_s_ic_setup._addr_reload_status,
			tmp_data, DATA_LEN_4);
		if (ret < NO_ERR) {
			E("%s: bus access fail!\n", __func__);
			return HW_CRC_FAIL;
		}

		if ((tmp_data[0] & 0x01) != 0x01) {
			ret = hx_s_core_fp._register_read(
				hx_s_ic_setup._addr_reload_crc32_result,
				tmp_data, DATA_LEN_4);
			if (ret < NO_ERR) {
				E("%s: bus access fail!\n", __func__);
				return HW_CRC_FAIL;
			}
			I("%s:data[3]=%X,data[2]=%X,data[1]=%X,data[0]=%X\n",
				__func__,
				tmp_data[3],
				tmp_data[2],
				tmp_data[1],
				tmp_data[0]);
			result = ((tmp_data[3] << 24)
					+ (tmp_data[2] << 16)
					+ (tmp_data[1] << 8)
					+ tmp_data[0]);
			goto END;
		} else {
			I("Waiting for HW ready!\n");
			usleep_range(1000, 1100);
			if (cnt >= 100)
				hx_s_core_fp._read_FW_status();
		}

	} while (cnt++ < 100);
END:
	return result;
}

void himax_mcu_set_reload_cmd(uint8_t *write_data, int idx,
		uint32_t cmd_from, uint32_t cmd_to, uint32_t cmd_beat)
{
	int index = idx * 12;
	int i;

	for (i = 3; i >= 0; i--) {
		write_data[index + i] = (cmd_from >> (8 * i));
		write_data[index + 4 + i] = (cmd_to >> (8 * i));
		write_data[index + 8 + i] = (cmd_beat >> (8 * i));
	}
}

bool himax_mcu_program_reload(void)
{
	return true;
}

#if defined(HX_ULTRA_LOW_POWER)
int himax_mcu_ulpm_in(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_write[DATA_LEN_4];
	int rtimes = 0;
	int ret = 0;

	I("%s:entering\n", __func__);

	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			I("%s:1/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_11,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_34,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_34, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x11=current 0x%2.2X\n",
				__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_11);

	rtimes = 0;
	/* 34 -> 11 */
	do {
		if (rtimes > 10) {
			I("%s:2/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_11,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_34,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_34, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x11=current 0x%2.2X\n",
				__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_11);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:3/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_33,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_33,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_33, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0x33=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_33);

	/* 34 -> 22 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:4/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_22,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_34,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_34, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x34,correct 0x22=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_22);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:5/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_aa,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_33,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_33, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33, correct 0xAA=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_aa);

	/* 33 -> 33 */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:6/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_33,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_33,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_33, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0x33=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_33);

	/* 33 -> AA */
	rtimes = 0;
	do {
		if (rtimes > 10) {
			I("%s:7/7 retry over 10 times!\n", __func__);
			return false;
		}
		hx_parse_assign_cmd(hx_s_ic_setup._data_ulpm_aa,
			tmp_write, DATA_LEN_1);
		ret = himax_bus_write(hx_s_ic_setup._addr_ulpm_33,
			NUM_NULL,
			tmp_write, 1);
		if (ret < 0) {
			I("%s: spi write fail!\n", __func__);
			continue;
		}
		ret = himax_bus_read(hx_s_ic_setup._addr_ulpm_33, tmp_data, 1);
		if (ret < 0) {
			I("%s: spi read fail!\n", __func__);
			continue;
		}

		I("%s:retry times %d,addr=0x33,correct 0xAA=current 0x%2.2X\n",
			__func__, rtimes, tmp_data[0]);
		rtimes++;
	} while (tmp_data[0] != hx_s_ic_setup._data_ulpm_aa);

	I("%s:END\n", __func__);
	return true;
}

int himax_mcu_black_gest_ctrl(bool enable)
{
	int ret = 0;

	I("%s:enable=%d, ts->is_suspended=%d\n", __func__,
			enable, hx_s_ts->suspended);

	if (hx_s_ts->suspended) {
		if (enable) {
#if defined(HX_RST_PIN_FUNC)
			hx_s_core_fp._pin_reset();
#else
			I("%s: Please enable TP reset define\n", __func__);
#endif
		} else {
			hx_s_core_fp._ulpm_in();
		}
	} else {
		hx_s_core_fp._sense_on(0);
	}
	return ret;
}
#endif

void himax_mcu_set_SMWP_enable(uint8_t SMWP_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (SMWP_enable) {
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				tmp_data, 4);
			hx_s_core_fp._register_write(hx_s_ic_setup._func_smwp,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				back_data, 4);
		} else {
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				tmp_data,
				4);
			hx_s_core_fp._register_write(hx_s_ic_setup._func_smwp,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				back_data,
				4);
		}
		hx_s_core_fp._register_read(hx_s_ic_setup._func_smwp, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],SMWP_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

void himax_mcu_set_HSEN_enable(uint8_t HSEN_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (HSEN_enable) {
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				tmp_data, 4);
			hx_s_core_fp._register_write(hx_s_ic_setup._func_hsen,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				back_data, 4);
		} else {
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				tmp_data,
				4);
			hx_s_core_fp._register_write(hx_s_ic_setup._func_hsen,
				tmp_data, DATA_LEN_4);

			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				back_data,
				4);
		}
		hx_s_core_fp._register_read(hx_s_ic_setup._func_hsen, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, HSEN_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],HSEN_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

#if defined(EARPHONE_PREVENT)
void himax_mcu_set_earphone_prevent_enable(uint8_t earphone_enable, bool suspended)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;
	I("TP_LOG:%s in\n", __func__);
	do {
		if (earphone_enable) {
			hx_parse_assign_cmd(hx_s_ic_setup._data_earphone_prevent,
				tmp_data, 4);
			hx_s_core_fp._register_write(hx_s_ic_setup._addr_earphone_prevent,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(hx_s_ic_setup._data_earphone_prevent,
				back_data, 4);
		} else {
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				tmp_data,
				4);
			hx_s_core_fp._register_write(hx_s_ic_setup._addr_earphone_prevent,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				back_data,
				4);
		}
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_earphone_prevent, tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],SMWP_enable,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
	I("TP_LOG:%s out\n", __func__);
}
#endif

void himax_mcu_usb_detect_set(uint8_t *cable_config)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t retry_cnt = 0;

	do {
		if (cable_config[1] == 0x01) {
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				tmp_data, 4);
			hx_s_core_fp._register_write(
				hx_s_ic_setup._func_usb_detect,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(hx_s_ic_setup._func_en,
				back_data, 4);
			I("%s: USB detect status IN!\n", __func__);
		} else {
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				tmp_data,
				4);
			hx_s_core_fp._register_write(
				hx_s_ic_setup._func_usb_detect,
				tmp_data, DATA_LEN_4);
			hx_parse_assign_cmd(
				hx_s_ic_setup._func_dis,
				back_data,
				4);
			I("%s: USB detect status OUT!\n", __func__);
		}

		hx_s_core_fp._register_read(hx_s_ic_setup._func_usb_detect,
			tmp_data,
			DATA_LEN_4);
		/*I("%s: tmp_data[0]=%d, USB detect=%d, retry_cnt=%d\n",
		 *	__func__, tmp_data[0],cable_config[1] ,retry_cnt);
		 */
		retry_cnt++;
	} while ((tmp_data[3] != back_data[3]
		|| tmp_data[2] != back_data[2]
		|| tmp_data[1] != back_data[1]
		|| tmp_data[0] != back_data[0])
		&& retry_cnt < HIMAX_REG_RETRY_TIMES);
}

#define PRT_DATA "%s:[3]=0x%2X, [2]=0x%2X, [1]=0x%2X, [0]=0x%2X\n"
void himax_mcu_diag_register_set(uint8_t diag_command,
		uint8_t storage_type, bool is_dirly)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t back_data[DATA_LEN_4];
	uint8_t cnt = 50;

	if (diag_command > 0 && storage_type % 8 > 0 && !is_dirly)
		tmp_data[0] = diag_command + 0x08;
	else
		tmp_data[0] = diag_command;
	I("diag_command = %d, tmp_data[0] = %X\n", diag_command, tmp_data[0]);
	hx_s_core_fp._interface_on();
	tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00;
	do {
		hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawout_sel,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_rawout_sel,
			back_data,
			DATA_LEN_4);
		I(PRT_DATA,	__func__,
			back_data[3],
			back_data[2],
			back_data[1],
			back_data[0]);
		cnt--;
	} while (tmp_data[0] != back_data[0] && cnt > 0);
	hx_s_ts->diag_storage_type = storage_type;
	hx_s_ts->diag_dirly = is_dirly;
}

#define PRT_TMP_DATA "%s:[0]=0x%2X,[1]=0x%2X,	[2]=0x%2X,[3]=0x%2X\n"
void himax_mcu_idle_mode(int disable)
{
	int retry = 20;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t switch_cmd = 0x00;

	I("%s:entering\n", __func__);

	do {
		I("%s,now %d times!\n", __func__, retry);
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_fw_mode_status,
			tmp_data, DATA_LEN_4);

		if (disable)
			switch_cmd = hx_s_ic_setup._para_idle_dis;
		else
			switch_cmd = hx_s_ic_setup._para_idle_en;

		tmp_data[0] = switch_cmd;
		hx_s_core_fp._register_write(hx_s_ic_setup._addr_fw_mode_status,
			tmp_data, DATA_LEN_4);

		hx_s_core_fp._register_read(hx_s_ic_setup._addr_fw_mode_status,
			tmp_data, DATA_LEN_4);
		I(PRT_TMP_DATA,
			__func__,
			tmp_data[0],
			tmp_data[1],
			tmp_data[2],
			tmp_data[3]);

		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[0] != switch_cmd) && retry > 0);

	I("%s: setting OK!\n", __func__);
}

void himax_mcu_reload_disable(int disable)
{
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:entering\n", __func__);

	if (disable) { /*reload disable*/
		hx_parse_assign_cmd(hx_s_ic_setup._data_fw_reload_dis,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(hx_s_ic_setup._addr_chk_fw_reload,
			tmp_data, DATA_LEN_4);
	} else { /*reload enable*/
		hx_parse_assign_cmd(hx_s_ic_setup._data_fw_reload_en,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(hx_s_ic_setup._addr_chk_fw_reload,
			tmp_data, DATA_LEN_4);
	}

	I("%s: setting OK!\n", __func__);
}

int himax_mcu_read_ic_trigger_type(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	int trigger_type = false;

	hx_s_core_fp._register_read(hx_s_ic_setup._addr_chk_irq_edge,
		tmp_data, DATA_LEN_4);

	if ((tmp_data[1] & 0x01) == 1)
		trigger_type = true;

	return trigger_type;
}

int himax_mcu_read_i2c_status(void)
{
	return i2c_error_count;
}

/* Please call this function after FW finish reload done */
void himax_mcu_read_FW_ver(void)
{
	uint8_t data[12] = {0};

	hx_s_core_fp._register_read(hx_s_ic_setup._addr_fw_ver,
		data, DATA_LEN_4);
	hx_s_ic_data->vendor_panel_ver =  data[0];
	hx_s_ic_data->vendor_fw_ver = data[1] << 8 | data[2];
	I("PANEL_VER : %X\n", hx_s_ic_data->vendor_panel_ver);
	I("FW_VER : %X\n", hx_s_ic_data->vendor_fw_ver);

	hx_s_core_fp._register_read(hx_s_ic_setup._addr_fw_cfg,
		data, DATA_LEN_4);
	hx_s_ic_data->vendor_config_ver = data[2] << 8 | data[3];
	/*I("CFG_VER : %X\n",hx_s_ic_data->vendor_config_ver);*/
	hx_s_ic_data->vendor_touch_cfg_ver = data[2];
	I("TOUCH_VER : %X\n", hx_s_ic_data->vendor_touch_cfg_ver);
	hx_s_ic_data->vendor_display_cfg_ver = data[3];
	I("DISPLAY_VER : %X\n", hx_s_ic_data->vendor_display_cfg_ver);
	hx_s_core_fp._register_read(hx_s_ic_setup._addr_fw_vendor, data,
		DATA_LEN_4);
	hx_s_ic_data->vendor_cid_maj_ver = data[2];
	hx_s_ic_data->vendor_cid_min_ver = data[3];
	I("CID_VER : %X\n", (hx_s_ic_data->vendor_cid_maj_ver << 8
			| hx_s_ic_data->vendor_cid_min_ver));

	hx_s_core_fp._register_read(hx_s_ic_setup._addr_cus_info, data, 12);
	memcpy(hx_s_ic_data->vendor_cus_info, data, 12);
	I("Cusomer ID = %s\n", hx_s_ic_data->vendor_cus_info);

	hx_s_core_fp._register_read(hx_s_ic_setup._addr_proj_info, data, 12);
	memcpy(hx_s_ic_data->vendor_proj_info, data, 12);
	I("Project ID = %s\n", hx_s_ic_data->vendor_proj_info);
//-P86801AA1 peiyuexiang.wt 20230703, add, hardware_info
	if (strstr(Lcm_name_tp,"lide_hx83102j_wt_dsi_vdo_90hz_hsd")) {
		snprintf(TP_name,HARDWARE_MAX_ITEM_LONGTH,"IC:hx83102j, Module:lide, FW:0x%04x",
			hx_s_ic_data->vendor_touch_cfg_ver);
	} else if (strstr(Lcm_name_tp,"starry_hx83102j_wt_dsi_vdo_90hz_mdt")) {
		snprintf(TP_name,HARDWARE_MAX_ITEM_LONGTH,"IC:hx83102j, Module:starry, FW:0x%04x",
			hx_s_ic_data->vendor_touch_cfg_ver);
	}
//-P86801AA1 peiyuexiang.wt 20230703, add, hardware_info
}

bool himax_mcu_read_event_stack(uint8_t *buf, uint32_t length)
{

	struct time_var t_start, t_end, t_delta;
	int len = length;
	int i2c_speed = 0;

	if (hx_s_ts->debug_log_level & BIT(2))
		time_func(&t_start);

	himax_bus_read(hx_s_ic_setup._addr_event_stack, buf, length);

	if (hx_s_ts->debug_log_level & BIT(2)) {
		time_func(&t_end);
		t_delta.tv_nsec = (t_end.tv_sec * 1000000000 + t_end.tv_nsec)
			- (t_start.tv_sec * 1000000000 + t_start.tv_nsec);

		i2c_speed = (len * 9 * 1000000
			/ (int)t_delta.tv_nsec) * 13 / 10;
		hx_s_ts->bus_speed = (int)i2c_speed;
	}

	return 1;
}

void himax_mcu_return_event_stack(void)
{
	int retry = 20;
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t target_data[DATA_LEN_4];


	I("%s:entering\n", __func__);

	do {
		I("now %d times!\n", retry);



		hx_parse_assign_cmd(hx_s_ic_setup._data_rawdata_end,
			tmp_data, DATA_LEN_4);
		hx_parse_assign_cmd(hx_s_ic_setup._data_rawdata_end,
			target_data, DATA_LEN_4);
		hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawdata_buf,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_rawdata_buf,
			tmp_data, DATA_LEN_4);
		retry--;
		usleep_range(10000, 11000);
	} while ((tmp_data[1] != target_data[1]
		&& tmp_data[0] != target_data[0])
		&& retry > 0);

	I("%s: End of setting!\n", __func__);
}

bool himax_mcu_calculateChecksum(bool change_iref, uint32_t size)
{
	uint32_t CRC_result = 0xFFFFFFFF;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s:Now size=%d\n", __func__, size);

	hx_parse_assign_cmd(hx_s_ic_setup._data_rawdata_end,
			tmp_data, DATA_LEN_4);

	CRC_result = hx_s_core_fp._calc_crc_by_hw(tmp_data, size);
	msleep(50);

	if (CRC_result != 0)
		I("%s: CRC Fail=%d\n", __func__, CRC_result);

	return (CRC_result == 0) ? true : false;
}

void himax_mcu_read_FW_status(void)
{
	uint8_t len = 0;
	uint8_t i = 0;
	uint8_t addr[4] = {0};
	uint8_t data[4] = {0};

	len = (uint8_t)(sizeof(dbg_reg_ary)/sizeof(uint32_t));

	for (i = 0; i < len; i++) {
		hx_parse_assign_cmd(dbg_reg_ary[i], addr, 4);
		hx_s_core_fp._register_read(dbg_reg_ary[i], data, DATA_LEN_4);

		I("reg[0-3] : 0x%08X = 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
		dbg_reg_ary[i], data[0], data[1], data[2], data[3]);
	}
}

void himax_mcu_irq_switch(int switch_on)
{
	if (switch_on) {
		if (hx_s_ts->use_irq)
			himax_int_enable(switch_on);
		else
			hrtimer_start(&hx_s_ts->timer, ktime_set(1, 0),
				HRTIMER_MODE_REL);

	} else {
		if (hx_s_ts->use_irq)
			himax_int_enable(switch_on);
		else {
			hrtimer_cancel(&hx_s_ts->timer);
			cancel_work_sync(&hx_s_ts->work);
		}
	}
}

int himax_mcu_assign_sorting_mode(uint8_t *tmp_data)
{
	uint8_t retry = 0;
	uint8_t rdata[4] = {0};

	I("%s:addr: 0x%08X, write to:0x%02X%02X%02X%02X\n",
		__func__,
		hx_s_ic_setup._addr_sorting_mode_en,
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);

	while (retry++ < 3) {
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_sorting_mode_en,
			tmp_data, DATA_LEN_4);
		usleep_range(1000, 1100);
		hx_s_core_fp._register_read(hx_s_ic_setup._addr_sorting_mode_en,
			rdata, DATA_LEN_4);

		if (rdata[3] == tmp_data[3] && rdata[2] == tmp_data[2]
		&& rdata[1] == tmp_data[1] && rdata[0] == tmp_data[0]) {
			I("%s: success to write sorting mode\n", __func__);
			return NO_ERR;
		}
		E("%s: fail to write sorting mode\n", __func__);

	}
	return BUS_FAIL;
}

int himax_mcu_check_sorting_mode(uint8_t *tmp_data)
{
	int ret = NO_ERR;

	ret = hx_s_core_fp._register_read(hx_s_ic_setup._addr_sorting_mode_en,
		tmp_data,
		DATA_LEN_4);
	I("%s:addr: 0x%08X, Now is:0x%02X%02X%02X%02X\n",
		__func__,
		hx_s_ic_setup._addr_sorting_mode_en,
		tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
	if (tmp_data[3] == 0xFF
		&& tmp_data[2] == 0xFF
		&& tmp_data[1] == 0xFF
		&& tmp_data[0] == 0xFF) {
		ret = BUS_FAIL;
		I("%s, All 0xFF, Fail!\n", __func__);
	}

	return ret;
}


void hx_ap_notify_fw_sus(int suspend)
{
	int retry = 0;
	int read_sts = 0;
	uint8_t read_tmp[DATA_LEN_4] = {0};
	uint8_t data_tmp[DATA_LEN_4] = {0};


	if (suspend) {
		I("%s,Suspend mode!\n", __func__);
		hx_parse_assign_cmd(hx_s_ic_setup._func_en,
			data_tmp, DATA_LEN_4);
	} else {
		I("%s,NonSuspend mode!\n", __func__);
		hx_parse_assign_cmd(hx_s_ic_setup._func_dis,
			data_tmp, DATA_LEN_4);
	}

	I("%s: R%08XH<-0x%02X%02X%02X%02X\n",
		__func__,
		hx_s_ic_setup._func_ap_notify_fw_sus,
		data_tmp[3], data_tmp[2], data_tmp[1], data_tmp[0]);
	do {
		hx_s_core_fp._register_write(
			hx_s_ic_setup._func_ap_notify_fw_sus,
			data_tmp,
			sizeof(data_tmp));
		usleep_range(1000, 1001);
		read_sts = hx_s_core_fp._register_read(
			hx_s_ic_setup._func_ap_notify_fw_sus,
			read_tmp,
			sizeof(read_tmp));
		I("%s: read bus status=%d\n", __func__, read_sts);
		I("%s: Now retry=%d, data=0x%02X%02X%02X%02X\n",
			__func__, retry,
			read_tmp[3], read_tmp[2], read_tmp[1], read_tmp[0]);
	} while ((retry++ < 10) && (read_sts != NO_ERR) &&
		(read_tmp[3] != data_tmp[3] && read_tmp[2] != data_tmp[2] &&
		read_tmp[1] != data_tmp[1] && read_tmp[0] != data_tmp[0]));
}
/* FW side end*/
/* CORE_FW */

/* CORE_FLASH */
/* FLASH side start*/
void himax_mcu_chip_erase(void)
{
	uint8_t tmp_data[DATA_LEN_4] = {0};

	hx_s_core_fp._interface_on();

	/* Reset power saving level */
	if (hx_s_core_fp._init_psl != NULL)
		hx_s_core_fp._init_psl();

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_trans_ctrl_2,
			tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
		tmp_data, DATA_LEN_4);

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_2,
			tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_flash_spi200_cmd,
		tmp_data, DATA_LEN_4);

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_3,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_flash_spi200_cmd,
		tmp_data, DATA_LEN_4);

	msleep(2000);

	if (!hx_s_core_fp._wait_wip(100))
		E("%s: Chip_Erase Fail\n", __func__);

}

bool himax_mcu_block_erase(int start_addr, int length)
{
	uint32_t page_prog_start = 0;
	uint32_t block_size = 0x10000;

	uint8_t tmp_data[DATA_LEN_4] = {0};

	hx_s_core_fp._interface_on();

	hx_s_core_fp._init_psl();

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);

	for (page_prog_start = start_addr;
	page_prog_start < start_addr + length;
	page_prog_start = page_prog_start + block_size) {

		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);

		tmp_data[3] = (page_prog_start >> 24)&0xFF;
		tmp_data[2] = (page_prog_start >> 16)&0xFF;
		tmp_data[1] = (page_prog_start >> 8)&0xFF;
		tmp_data[0] = page_prog_start&0xFF;
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_addr,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_3,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_4,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);
		msleep(1000);

		if (!hx_s_core_fp._wait_wip(100)) {
			E("%s:Erase Fail\n", __func__);
			return false;
		}
	}

	I("%s:END\n", __func__);
	return true;
}

bool himax_mcu_sector_erase(int start_addr)
{
	return true;
}

bool himax_mcu_flash_programming(uint8_t *FW_content, int FW_Size)
{
	int page_prog_start = 0;
	uint8_t tmp_data[DATA_LEN_4];
	int ret = 0;

	/* 4 bytes for padding*/
	hx_s_core_fp._interface_on();

	hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_flash_spi200_trans_fmt,
		tmp_data, DATA_LEN_4);

	for (page_prog_start = 0; page_prog_start < FW_Size;
	page_prog_start += FLASH_RW_MAX_LEN) {

		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);

	    /* ===WEL Write Control ===*/
		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_6,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data,
			DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_1,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
				tmp_data,
				DATA_LEN_4);

		hx_s_core_fp._register_read(
			hx_s_ic_setup._addr_flash_spi200_data,
				tmp_data,
				DATA_LEN_4);

		/* === Check WEL Fail ===*/
		if (((tmp_data[0] & 0x02) >> 1) == 0) {
			I("%s:SPI 0x8000002c = %d, Check WEL Fail\n", __func__,
				tmp_data[0]);
			return false;
		}

		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_2,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);



		/*Programmable size = 1 page = 256 bytes,*/
		/*word_number = 256 byte / 4 = 64*/
		hx_parse_assign_cmd(
			hx_s_ic_setup._data_flash_spi200_trans_ctrl_4,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_trans_ctrl,
			tmp_data, DATA_LEN_4);

		/* Flash start address 1st : 0x0000_0000*/
		if (page_prog_start < 0x100) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x100
		&& page_prog_start < 0x10000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		} else if (page_prog_start >= 0x10000
		&& page_prog_start < 0x1000000) {
			tmp_data[3] = 0x00;
			tmp_data[2] = (uint8_t)(page_prog_start >> 16);
			tmp_data[1] = (uint8_t)(page_prog_start >> 8);
			tmp_data[0] = (uint8_t)page_prog_start;
		}
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_addr,
			tmp_data, DATA_LEN_4);

		ret = hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_data,
			&FW_content[page_prog_start], 16);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		hx_parse_assign_cmd(hx_s_ic_setup._data_flash_spi200_cmd_6,
			tmp_data, DATA_LEN_4);
		hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_cmd,
			tmp_data, DATA_LEN_4);

		ret = hx_s_core_fp._register_write(
			hx_s_ic_setup._addr_flash_spi200_data,
			&FW_content[page_prog_start+16], 240);
		if (ret < 0) {
			E("%s: bus access fail!\n", __func__);
			return false;
		}

		if (!hx_s_core_fp._wait_wip(1)) {
			E("%s:Flash_Programming Fail\n", __func__);
			return false;
		}

	}
	return true;
}
void himax_mcu_flash_page_write(uint8_t *write_addr, int length,
		uint8_t *write_data)
{
}

static void himax_flash_speed_set(uint8_t speed)
{
	uint8_t tmp_data[4];

	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x02;/*extand cs high to 100ns*/
	tmp_data[0] = speed;

	hx_s_core_fp._register_write(
		hx_s_ic_setup._addr_flash_spi200_flash_speed,
		tmp_data, 4);
}

int himax_mcu_fts_ctpm_fw_upgrade_with_sys_fs(unsigned char *fw,
		int len, bool change_iref)
{
	int burnFW_success = 0;
	int counter = 0;
	uint8_t tmp_addr[DATA_LEN_4] = {0};
	uint8_t tmp_data[DATA_LEN_4] = {0};

	I("%s:Now Entering,size=%d\n", __func__, len);


	hx_s_core_fp._ic_reset(0);

	for (counter = 0; counter < 3; counter++) {
		hx_s_core_fp._sense_off(true);
		himax_flash_speed_set(HX_FLASH_SPEED_12p5M);
		hx_s_core_fp._block_erase(0x00, len);
		if (!hx_s_core_fp._flash_programming(fw, len)) {
			hx_s_core_fp._ic_reset(0);

			continue;
		}

		hx_parse_assign_cmd(hx_s_ic_setup._addr_program_reload_from,
			tmp_addr, DATA_LEN_4);
		if (hx_s_core_fp._calc_crc_by_hw(tmp_addr,
			len) == 0) {
			burnFW_success = 1;
			break;
		}
	}

	/*RawOut select initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawout_sel,
		tmp_data, DATA_LEN_4);
	/*DSRAM func initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._assign_sorting_mode(tmp_data);


	return burnFW_success;
}

void himax_mcu_flash_dump_func(uint8_t local_flash_command,
		int Flash_Size, uint8_t *flash_buffer)
{
	uint32_t page_prog_start = 0;

	hx_s_core_fp._sense_off(true);

	for (page_prog_start = 0; page_prog_start < Flash_Size;
	page_prog_start += 128) {
		hx_s_core_fp._register_read(page_prog_start,
		flash_buffer+page_prog_start, 128);
	}

	hx_s_core_fp._sense_on(0x01);
}

bool himax_mcu_flash_lastdata_check(uint32_t size)
{
	/* 64K - 0x80, which is the address of
	 * the last 128bytes in 64K, default value
	 */
	uint32_t start_addr = 0xFFFFFFFF;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128];

	if (size < flash_page_len) {
		E("%s: flash size is wrong, terminated\n", __func__);
		E("%s: flash size = %08X; flash page len = %08X\n", __func__,
			size, flash_page_len);
		goto FAIL;
	}

	/* In order to match other size of fw */
	start_addr = size - flash_page_len;
	I("%s: Now size is %d, the start_addr is 0x%08X\n",
			__func__, size, start_addr);
	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len);
	temp_addr = temp_addr + flash_page_len) {

		hx_s_core_fp._register_read(temp_addr, &flash_tmp_buffer[0],
			flash_page_len);
	}

	I("FLASH[%08X] ~ FLASH[%08X] = %02X%02X%02X%02X\n", size-4, size-1,
		flash_tmp_buffer[flash_page_len-4],
		flash_tmp_buffer[flash_page_len-3],
		flash_tmp_buffer[flash_page_len-2],
		flash_tmp_buffer[flash_page_len-1]);

	if ((!flash_tmp_buffer[flash_page_len-4])
	&& (!flash_tmp_buffer[flash_page_len-3])
	&& (!flash_tmp_buffer[flash_page_len-2])
	&& (!flash_tmp_buffer[flash_page_len-1])) {
		I("Fail, Last four Bytes are all 0x00:\n");
		goto FAIL;
	} else if ((flash_tmp_buffer[flash_page_len-4] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-3] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-2] == 0xFF)
	&& (flash_tmp_buffer[flash_page_len-1] == 0xFF)) {
		I("Fail, Last four Bytes are all 0xFF:\n");
		goto FAIL;
	} else {
		return 0;
	}

FAIL:
	return 1;
}

bool hx_bin_desc_data_get(uint32_t addr, uint8_t *flash_buf)
{
	uint8_t data_sz = 0x10;
	uint32_t i = 0, j = 0;
	uint16_t chk_end = 0;
	uint16_t chk_sum = 0;
	uint32_t map_code = 0;
	unsigned long flash_addr = 0;

	for (i = 0; i < FW_PAGE_SZ; i = i + data_sz) {
		for (j = i; j < (i + data_sz); j++) {
			chk_end |= flash_buf[j];
			chk_sum += flash_buf[j];
		}
		if (!chk_end) { /*1. Check all zero*/
			I("%s: End in %X\n",	__func__, i + addr);
			return false;
		} else if (chk_sum % 0x100) { /*2. Check sum*/
			I("%s: chk sum failed in %X\n",	__func__, i + addr);
		} else { /*3. get data*/
			map_code = flash_buf[i] + (flash_buf[i + 1] << 8)
			+ (flash_buf[i + 2] << 16) + (flash_buf[i + 3] << 24);
			flash_addr = flash_buf[i + 4] + (flash_buf[i + 5] << 8)
			+ (flash_buf[i + 6] << 16) + (flash_buf[i + 7] << 24);
			switch (map_code) {
			case FW_CID:
				CID_VER_MAJ_FLASH_ADDR = flash_addr;
				CID_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: CID_VER in %lX\n", __func__,
				CID_VER_MAJ_FLASH_ADDR);
				break;
			case FW_VER:
				FW_VER_MAJ_FLASH_ADDR = flash_addr;
				FW_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: FW_VER in %lX\n", __func__,
				FW_VER_MAJ_FLASH_ADDR);
				break;
			case CFG_VER:
				CFG_VER_MAJ_FLASH_ADDR = flash_addr;
				CFG_VER_MIN_FLASH_ADDR = flash_addr + 1;
				I("%s: CFG_VER in = %08lX\n", __func__,
				CFG_VER_MAJ_FLASH_ADDR);
				break;
			case TP_CONFIG_TABLE:
				CFG_TABLE_FLASH_ADDR = flash_addr;
				I("%s: CONFIG_TABLE in %X\n",
				__func__, CFG_TABLE_FLASH_ADDR);
				break;
			}
		}
		chk_end = 0;
		chk_sum = 0;
	}

	return true;
}

bool hx_mcu_bin_desc_get(unsigned char *fw, uint32_t max_sz)
{
	uint32_t addr_t = 0;
	unsigned char *fw_buf = NULL;
	bool keep_on_flag = false;
	bool g_bin_desc_flag = false;

	do {
		fw_buf = &fw[addr_t];

		/*Check bin is with description table or not*/
		if (!g_bin_desc_flag) {
			if (fw_buf[0x00] == 0x00 && fw_buf[0x01] == 0x00
			&& fw_buf[0x02] == 0x00 && fw_buf[0x03] == 0x00
			&& fw_buf[0x04] == 0x00 && fw_buf[0x05] == 0x00
			&& fw_buf[0x06] == 0x00 && fw_buf[0x07] == 0x00
			&& fw_buf[0x0E] == 0x87)
				g_bin_desc_flag = true;
		}
		if (!g_bin_desc_flag) {
			I("%s: fw_buf[0x00] = %2X, fw_buf[0x0E] = %2X\n",
			__func__, fw_buf[0x00], fw_buf[0x0E]);
			I("%s: No description table\n",	__func__);
			break;
		}

		/*Get related data*/
		keep_on_flag = hx_s_core_fp._bin_desc_data_get(addr_t, fw_buf);

		addr_t = addr_t + FW_PAGE_SZ;
	} while (max_sz > addr_t && keep_on_flag);

	return g_bin_desc_flag;
}

int hx_mcu_diff_overlay_flash(void)
{
	int rslt = 0;
	int diff_val = 0;

	diff_val = (hx_s_ic_data->vendor_fw_ver);
	I("%s:Now fw ID is 0x%04X\n", __func__, diff_val);
	diff_val = (diff_val >> 12);
	I("%s:Now diff value=0x%04X\n", __func__, diff_val);

	if (diff_val == 1)
		I("%s:Now size should be 128K!\n", __func__);
	else
		I("%s:Now size should be 64K!\n", __func__);
	rslt = diff_val;
	return rslt;
}

/* FLASH side end*/
/* CORE_FLASH */

/* CORE_SRAM */
/* SRAM side start*/

bool himax_mcu_get_DSRAM_data(uint8_t *info_data, bool DSRAM_Flag)
{
	unsigned int i = 0;
	unsigned char tmp_data[DATA_LEN_4];
	unsigned char end_data[DATA_LEN_4];
	unsigned int max_bus_size = MAX_I2C_TRANS_SZ;
	uint8_t x_num = hx_s_ic_data->rx_num;
	uint8_t y_num = hx_s_ic_data->tx_num;
	/*int m_key_num = 0;*/
	unsigned int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
	unsigned int data_size = (x_num * y_num + x_num + y_num) * 2;
	unsigned int remain_size;
	uint8_t retry = 0;
	/*int mutual_data_size = x_num * y_num * 2;*/
	unsigned int addr = 0;
	uint8_t  *temp_info_data = NULL; /*max mkey size = 8*/
	uint32_t checksum = 0;
	int fw_run_flag = -1;

#if defined(BUS_R_DLEN)
	max_bus_size = BUS_R_DLEN;
#endif

	if (strcmp(hx_s_ts->chip_name, HX_83121A_SERIES_PWON) == 0) {
		if (max_bus_size > HX4K)
			max_bus_size = HX4K;
	}

	temp_info_data = kcalloc((total_size + 8), sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate memory\n", __func__);
		return false;
	}
	/* 1. Read number of MKey R100070E8H to determin data size */
	/* m_key_num = hx_s_ic_data->bt_num; */
	/* I("%s,m_key_num=%d\n",__func__ ,m_key_num); */
	/* total_size += m_key_num * 2; */

	/* 2. Start DSRAM Rawdata and Wait Data Ready */
	hx_parse_assign_cmd(hx_s_ic_setup._pwd_get_rawdata_start,
			tmp_data, DATA_LEN_4);
	hx_parse_assign_cmd(hx_s_ic_setup._pwd_get_rawdata_end,
			end_data, DATA_LEN_4);
	fw_run_flag = himax_write_read_reg(hx_s_ic_setup._addr_rawdata_buf,
			tmp_data,
			end_data[1],
			end_data[0]);

	if (fw_run_flag < 0) {
		E("%s: Data NOT ready => bypass\n", __func__);
		kfree(temp_info_data);
		return false;
	}

#if defined(HX_STRESS_SELF_TEST)
	if (hx_s_ts->in_self_test == 1)
		hx_s_core_fp._sense_off(true);
#endif

	/* 3. Read RawData */
	while (retry++ < 5) {
		remain_size = total_size;
		while (remain_size > 0) {

			i = total_size - remain_size;
			addr = hx_s_ic_setup._addr_rawdata_buf + i;


			if (remain_size >= max_bus_size) {
				hx_s_core_fp._register_read(addr,
					&temp_info_data[i], max_bus_size);
				remain_size -= max_bus_size;
			} else {
				hx_s_core_fp._register_read(addr,
					&temp_info_data[i], remain_size);
				remain_size = 0;
			}
		}

		/* 5. Data Checksum Check */
		/* 2 is meaning PASSWORD NOT included */
		checksum = 0;
		for (i = 2; i < total_size; i += 2)
			checksum += temp_info_data[i+1]<<8 | temp_info_data[i];

		if (checksum % 0x10000 != 0) {

			E("%s: check_sum_cal fail=%08X\n", __func__, checksum);

		} else {
			memcpy(info_data, &temp_info_data[4],
				data_size * sizeof(uint8_t));
			break;
		}
	}

	/* 4. FW stop outputing */
	tmp_data[3] = temp_info_data[3];
	tmp_data[2] = temp_info_data[2];
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawdata_buf,
		tmp_data, DATA_LEN_4);

	kfree(temp_info_data);
	if (retry >= 5)
		return false;
	else
		return true;

}

/* SRAM side end*/
/* CORE_SRAM */

/* CORE_DRIVER */


void himax_mcu_init_ic(void)
{
	I("%s: use default init.\n", __func__);
}

void himax_suspend_proc(bool suspended)
{
	I("%s: Entering!\n", __func__);
}

void himax_resume_proc(bool suspended)
{
#if defined(HX_ZERO_FLASH)
	int result = 0;
#endif

	I("%s: Entering!\n", __func__);
#if defined(HX_ZERO_FLASH)
	if (hx_s_core_fp._0f_op_file_dirly != NULL) {
		result = hx_s_core_fp._0f_op_file_dirly(g_fw_boot_upgrade_name);
		if (result)
			E("%s: update FW fail, code[%d]!!\n", __func__, result);
	}
#else
	if (hx_s_core_fp._resend_cmd_func != NULL)
		hx_s_core_fp._resend_cmd_func(suspended);
#endif

	if (hx_s_core_fp._ap_notify_fw_sus != NULL)
		hx_s_core_fp._ap_notify_fw_sus(0);
}


#if defined(HX_RST_PIN_FUNC)
void himax_mcu_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	himax_rst_gpio_set(hx_s_ts->rst_gpio, 0);
	usleep_range(RST_LOW_PERIOD_S, RST_LOW_PERIOD_E);
	himax_rst_gpio_set(hx_s_ts->rst_gpio, 1);
	usleep_range(RST_HIGH_PERIOD_S, RST_HIGH_PERIOD_E);
}
#endif
void himax_mcu_ic_reset(int level)
{
	// struct himax_ts_data *ts = hx_s_ts;

	g_hw_rst_activate = 0;
	I("%s,status: reset level = %d\n", __func__,
			level);
	switch (level) {
	case 0:
#if defined(HX_RST_PIN_FUNC)
		himax_mcu_pin_reset();
#else
		if (hx_s_core_fp._system_reset != NULL)
			hx_s_core_fp._system_reset();
		else
			E("%s: There is no definition for system reset!\n",
				__func__);
#endif
		break;
	case 1:
		if (hx_s_core_fp._system_reset != NULL)
			hx_s_core_fp._system_reset();
		else
			E("%s: There is no definition for system reset!\n",
				__func__);
		break;
	case 2:
		hx_s_core_fp._leave_safe_mode();
		break;
	case 3:
		hx_s_core_fp._sense_on(0x01);
		break;
	default:
		break;
	}
}


uint8_t himax_mcu_tp_info_check(void)
{
	char addr[DATA_LEN_4] = {0};
	char data[DATA_LEN_4] = {0};
	uint32_t rx_num;
	uint32_t tx_num;
	uint32_t bt_num;
	uint32_t max_pt;
	uint8_t int_is_edge;
	uint8_t stylus_func;
	uint8_t stylus_id_v2;
	uint8_t stylus_ratio;
	uint8_t err_cnt = 0;

	hx_parse_assign_cmd(hx_s_ic_setup._addr_info_channel_num,
		addr, DATA_LEN_4);
	hx_s_core_fp._register_read(hx_s_ic_setup._addr_info_channel_num,
		data, DATA_LEN_4);
	rx_num = data[2];
	tx_num = data[3];

	hx_parse_assign_cmd(hx_s_ic_setup._addr_info_max_pt, addr, DATA_LEN_4);
	hx_s_core_fp._register_read(hx_s_ic_setup._addr_info_max_pt,
		data, DATA_LEN_4);
	max_pt = data[0];

	hx_parse_assign_cmd(hx_s_ic_setup._addr_chk_irq_edge, addr, DATA_LEN_4);
	hx_s_core_fp._register_read(hx_s_ic_setup._addr_chk_irq_edge,
		data, DATA_LEN_4);
	if ((data[1] & 0x01) == 1)
		int_is_edge = true;
	else
		int_is_edge = false;

	/*1. Read number of MKey R100070E8H to determin data size*/
	hx_parse_assign_cmd(hx_s_ic_setup._addr_mkey, addr, DATA_LEN_4);
	hx_s_core_fp._register_read(hx_s_ic_setup._addr_mkey, data, DATA_LEN_4);

	bt_num = data[0] & 0x03;


	hx_s_core_fp._register_read(hx_s_ic_setup._addr_info_def_stylus,
		data, DATA_LEN_4);
	stylus_func = data[3];

	if (hx_s_ic_data->rx_num != rx_num) {
		err_cnt++;
		W("%s: RX_NUM, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->rx_num, rx_num);
	}

	if (hx_s_ic_data->tx_num != tx_num) {
		err_cnt++;
		W("%s: TX_NUM, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->tx_num, tx_num);
	}

	if (hx_s_ic_data->bt_num != bt_num) {
		err_cnt++;
		W("%s: BT_NUM, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->bt_num, bt_num);
	}

	if (hx_s_ic_data->max_pt != max_pt) {
		err_cnt++;
		W("%s: MAX_PT, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->max_pt, max_pt);
	}

	if (hx_s_ic_data->int_is_edge != int_is_edge) {
		err_cnt++;
		W("%s: INT_IS_EDGE, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->int_is_edge, int_is_edge);
	}

	if (hx_s_ic_data->stylus_func != stylus_func) {
		err_cnt++;
		W("%s: STYLUS_FUNC, Set = %d ; FW = %d", __func__,
			hx_s_ic_data->stylus_func, stylus_func);
	}

	if (hx_s_ic_data->stylus_func) {
		hx_s_core_fp._register_read(
			hx_s_ic_setup._addr_info_stylus_ratio,
			data, DATA_LEN_4);
		stylus_id_v2 = data[2];/*0x100071FE 0=off 1=on*/
		stylus_ratio = data[3];
		/*0x100071FF 0=ratio_1 10=ratio_10*/
		if (hx_s_ic_data->stylus_id_v2 != stylus_id_v2) {
			err_cnt++;
			W("%s: STYLUS_ID_V2, Set = %d ; FW = %d", __func__,
				hx_s_ic_data->stylus_id_v2, stylus_id_v2);
		}
		if (hx_s_ic_data->stylus_ratio != stylus_ratio) {
			err_cnt++;
			W("%s: STYLUS_RATIO, Set = %d ; FW = %d", __func__,
				hx_s_ic_data->stylus_ratio, stylus_ratio);
		}
	}

	if (err_cnt > 0)
		W("FIX_TOUCH_INFO does NOT match to FW information\n");
	else
		I("FIX_TOUCH_INFO is OK\n");

	return err_cnt;
}

void himax_mcu_touch_information(void)
{
// +P86801AA1 peiyuexiang.wt,add,20230704,compatible with lide
	if (strstr(Lcm_name_tp,"lide_hx83102j_wt_dsi_vdo_90hz_hsd")) {
		if (hx_s_ic_data->rx_num == 0xFFFFFFFF)
			hx_s_ic_data->rx_num = FIX_HX_RX_NUM;
		if (hx_s_ic_data->tx_num == 0xFFFFFFFF)
			hx_s_ic_data->tx_num = FIX_HX_TX_NUM;
	} else if (strstr(Lcm_name_tp,"starry_hx83102j_wt_dsi_vdo_90hz_mdt")){
		if (hx_s_ic_data->rx_num == 0xFFFFFFFF)
			hx_s_ic_data->rx_num = FIX_HX_RX_NUM_MDT;
		if (hx_s_ic_data->tx_num == 0xFFFFFFFF)
			hx_s_ic_data->tx_num = FIX_HX_TX_NUM_MDT;
	} else {
		E("%s:compare cmdline error", __func__);
	}
// -P86801AA1 peiyuexiang.wt,add,20230704,compatible with lide

	if (hx_s_ic_data->bt_num == 0xFFFFFFFF)
		hx_s_ic_data->bt_num = FIX_HX_BT_NUM;

	if (hx_s_ic_data->max_pt == 0xFFFFFFFF)
		hx_s_ic_data->max_pt = FIX_HX_MAX_PT;

	if (hx_s_ic_data->int_is_edge == 0xFF)
		hx_s_ic_data->int_is_edge = FIX_HX_INT_IS_EDGE;

	if (hx_s_ic_data->stylus_func == 0xFF)
		hx_s_ic_data->stylus_func = FIX_HX_STYLUS_FUNC;

	if (hx_s_ic_data->stylus_id_v2 == 0xFF)
		hx_s_ic_data->stylus_id_v2 = FIX_HX_STYLUS_ID_V2;

	if (hx_s_ic_data->stylus_ratio == 0xFF)
		hx_s_ic_data->stylus_ratio = FIX_HX_STYLUS_RATIO;

	hx_s_ic_data->y_res = hx_s_ts->pdata->screenHeight;
	hx_s_ic_data->x_res = hx_s_ts->pdata->screenWidth;

	I("%s:rx_num =%d,tx_num =%d\n", __func__,
		hx_s_ic_data->rx_num, hx_s_ic_data->tx_num);
	I("%s:max_pt=%d\n", __func__, hx_s_ic_data->max_pt);
	I("%s:HX_Y_RES=%d,HX_X_RES =%d\n", __func__,
		hx_s_ic_data->y_res, hx_s_ic_data->x_res);
	I("%s:int_is_edge =%d,stylus_func = %d\n", __func__,
	hx_s_ic_data->int_is_edge, hx_s_ic_data->stylus_func);
		I("%s:HX_STYLUS_ID_V2 =%d,HX_STYLUS_RATIO = %d\n", __func__,
	hx_s_ic_data->stylus_id_v2, hx_s_ic_data->stylus_ratio);
}

void himax_mcu_calcTouchDataSize(void)
{
	struct himax_ts_data *ts_data = hx_s_ts;

	ts_data->x_channel = hx_s_ic_data->rx_num;
	ts_data->y_channel = hx_s_ic_data->tx_num;
	ts_data->nFinger_support = hx_s_ic_data->max_pt;
#if defined(SEC_PALM_FUNC)	
	ts_data->coord_data_size = 4 * ts_data->nFinger_support;
    ts_data->area_data_size = ((ts_data->nFinger_support / 4) + (ts_data->nFinger_support % 4 ? 1 : 0)) * 4;
    ts_data->coordInfoSize = ts_data->coord_data_size + ts_data->area_data_size + 4;
#endif
	HX_TOUCH_INFO_POINT_CNT = hx_s_ic_data->max_pt * 4;
	if ((hx_s_ic_data->max_pt % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT +=
			(hx_s_ic_data->max_pt / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT +=
			((hx_s_ic_data->max_pt / 4) + 1) * 4;

	if (himax_report_data_init())
		E("%s: allocate data fail\n", __func__);
}

int himax_mcu_get_touch_data_size(void)
{
	return HIMAX_TOUCH_DATA_SIZE;
}

int himax_mcu_hand_shaking(void)
{
	/* 0:Running, 1:Stop, 2:I2C Fail */
	int result = 0;
	return result;
}

int himax_mcu_determin_diag_rawdata(int diag_command)
{
	return diag_command % 10;
}

int himax_mcu_determin_diag_storage(int diag_command)
{
	return diag_command / 10;
}

int himax_mcu_cal_data_len(int raw_cnt_rmd, int max_pt,
		int raw_cnt_max)
{
	int RawDataLen;
	/* rawdata checksum is 2 bytes */
	if (raw_cnt_rmd != 0x00)
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((max_pt + raw_cnt_max + 3) * 4) - 2;
	else
		RawDataLen = MAX_I2C_TRANS_SZ
			- ((max_pt + raw_cnt_max + 2) * 4) - 2;

	return RawDataLen;
}

bool himax_mcu_diag_check_sum(struct himax_report_data *hx_s_touch_data)
{
	uint16_t check_sum_cal = 0;
	int i;

	/* Check 128th byte CRC */
	for (i = 0, check_sum_cal = 0;
	i < (hx_s_touch_data->touch_all_size
	- hx_s_touch_data->touch_info_size);
	i += 2) {
		check_sum_cal += (hx_s_touch_data->rawdata_buf[i + 1]
			* FLASH_RW_MAX_LEN
			+ hx_s_touch_data->rawdata_buf[i]);
	}

	if (check_sum_cal % HX64K != 0) {
		I("%s fail=%2X\n", __func__, check_sum_cal);
		return 0;
	}

	return 1;
}

int himax_mcu_diag_parse_raw_data(
		struct himax_report_data *hx_s_touch_data,
		int mul_num, int self_num, uint8_t diag_cmd,
		int32_t *mutual_data, int32_t *self_data)
{
	return	diag_mcu_parse_raw_data(hx_s_touch_data, mul_num, self_num,
			diag_cmd, mutual_data, self_data);
}

#if defined(HX_EXCP_RECOVERY)
int himax_mcu_ic_excp_recovery(uint32_t hx_excp_event,
		uint32_t hx_zero_event, uint32_t length)
{
	int ret_val = NO_ERR;

	if (hx_excp_event == length) {
		g_zero_event_count = 0;
		ret_val = HX_EXCP_EVENT;
	} else if (hx_zero_event == length) {
		if (g_zero_event_count > 5) {
			g_zero_event_count = 0;
			I("EXCEPTION event checked - ALL Zero.\n");
			ret_val = HX_EXCP_EVENT;
		} else {
			g_zero_event_count++;
			I("ALL Zero event is %d times.\n",
					g_zero_event_count);
			ret_val = HX_ZERO_EVENT_COUNT;
		}
	}

	return ret_val;
}

void himax_mcu_excp_ic_reset(void)
{
	I("%s:\n", __func__);

	HX_EXCP_RESET_ACTIVATE = 0;

	hx_s_core_fp._ic_reset(0);

}
#endif
#if defined(HX_TP_PROC_GUEST_INFO)
char *g_checksum_str = "check sum fail";
/* char *g_guest_info_item[] = {
 *	"projectID",
 *	"CGColor",
 *	"BarCode",
 *	"Reserve1",
 *	"Reserve2",
 *	"Reserve3",
 *	"Reserve4",
 *	"Reserve5",
 *	"VCOM",
 *	"Vcom-3Gar",
 *	NULL
 * };
 */

int himax_guest_info_get_status(void)
{
	return g_guest_info_data->g_guest_info_ongoing;
}
void himax_guest_info_set_status(int setting)
{
	g_guest_info_data->g_guest_info_ongoing = setting;
}

static int himax_guest_info_read(uint32_t start_addr,
		uint8_t *flash_tmp_buffer)
{
	uint32_t temp_addr = 0;
	uint8_t tmp_addr[4];
	uint32_t flash_page_len = 0x1000;
	/* uint32_t checksum = 0x00; */
	int result = -1;


	I("%s:Reading guest info in start_addr = 0x%08X !\n", __func__,
		start_addr);

	tmp_addr[0] = start_addr % 0x100;
	tmp_addr[1] = (start_addr >> 8) % 0x100;
	tmp_addr[2] = (start_addr >> 16) % 0x100;
	tmp_addr[3] = start_addr / 0x1000000;
	I("addr[0]=0x%2X,addr[1]=0x%2X,addr[2]=0x%2X,addr[3]=0x%2X\n",
		tmp_addr[0], tmp_addr[1],
		tmp_addr[2], tmp_addr[3]);

	result = hx_s_core_fp._calc_crc_by_hw(tmp_addr, flash_page_len);
	I("Checksum = 0x%8X\n", result);
	if (result != 0)
		goto END_FUNC;

	for (temp_addr = start_addr;
	temp_addr < (start_addr + flash_page_len);
	temp_addr = temp_addr + 128) {

		/* I("temp_addr=%d,tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,
		 *	tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",
		 *	temp_addr,tmp_addr[0],tmp_addr[1],
		 *	tmp_addr[2],tmp_addr[3]);
		 */
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		hx_s_core_fp._register_read(temp_addr,
			&flash_tmp_buffer[temp_addr - start_addr], 128);
		/* memcpy(&flash_tmp_buffer[temp_addr - start_addr],
		 *	buffer,128);
		 */
	}

END_FUNC:
	return result;
}

int hx_read_guest_info(void)
{
	uint32_t panel_info_addr = HX_GUEST_INFO_FLASH_SADDR;

	uint32_t info_len;
	uint32_t flash_page_len = 0x1000;/*4k*/
	uint8_t *flash_tmp_buffer = NULL;
	/* uint32_t temp_addr = 0; */
	uint8_t temp_str[128];
	int i = 0;
	unsigned int custom_info_temp = 0;
	int checksum = 0;

	himax_guest_info_set_status(1);

	flash_tmp_buffer = kcalloc(HX_GUEST_INFO_SIZE * flash_page_len,
		sizeof(uint8_t), GFP_KERNEL);
	if (flash_tmp_buffer == NULL) {
		I("%s: Memory allocate fail!\n", __func__);
		return MEM_ALLOC_FAIL;
	}

	hx_s_core_fp._sense_off(true);
	/* hx_s_core_fp._burst_enable(1); */

	for (custom_info_temp = 0;
	custom_info_temp < HX_GUEST_INFO_SIZE;
	custom_info_temp++) {
		checksum = himax_guest_info_read(panel_info_addr
			+ custom_info_temp
			* flash_page_len,
			&flash_tmp_buffer[custom_info_temp * flash_page_len]);
		if (checksum != 0) {
			E("%s:Checksum Fail! g_checksum_str len=%d\n", __func__,
				(int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str_in_format[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			memcpy(&g_guest_info_data->
				g_guest_str[custom_info_temp][0],
				g_checksum_str, (int)strlen(g_checksum_str));
			continue;
		}

		info_len = flash_tmp_buffer[custom_info_temp * flash_page_len]
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 1] << 8)
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 2] << 16)
			+ (flash_tmp_buffer[custom_info_temp
			* flash_page_len + 3] << 24);

		I("Now custom_info_temp = %d\n", custom_info_temp);

		I("Now size_buff[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
			flash_tmp_buffer[custom_info_temp*flash_page_len],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 1],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 2],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 3]);

		I("Now total length=%d\n", info_len);

		g_guest_info_data->g_guest_data_len[custom_info_temp] =
			info_len;

		I("Now custom_info_id [0]=%d,[1]=%d,[2]=%d,[3]=%d\n",
			flash_tmp_buffer[custom_info_temp*flash_page_len + 4],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 5],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 6],
			flash_tmp_buffer[custom_info_temp*flash_page_len + 7]);

		g_guest_info_data->g_guest_data_type[custom_info_temp] =
			flash_tmp_buffer[custom_info_temp * flash_page_len
			+ 7];

		/* if(custom_info_temp < 3) { */
		if (info_len > 128) {
			I("%s: info_len=%d\n", __func__, info_len);
			info_len = 128;
		}
		for (i = 0; i < info_len; i++)
			temp_str[i] = flash_tmp_buffer[custom_info_temp
				* flash_page_len
				+ HX_GUEST_INFO_LEN_SIZE
				+ HX_GUEST_INFO_ID_SIZE
				+ i];

		I("g_guest_info_data->g_guest_str_in_format[%d]size=%d\n",
			custom_info_temp, info_len);
		memcpy(&g_guest_info_data->
			g_guest_str_in_format[custom_info_temp][0],
			temp_str, info_len);
		/*}*/

		for (i = 0; i < 128; i++)
			temp_str[i] = flash_tmp_buffer[custom_info_temp
				* flash_page_len
				+ i];

		I("g_guest_info_data->g_guest_str[%d] size = %d\n",
				custom_info_temp, 128);
		memcpy(&g_guest_info_data->g_guest_str[custom_info_temp][0],
				temp_str, 128);
		/*if(custom_info_temp == 0)
		 *{
		 *	for ( i = 0; i< 256 ; i++) {
		 *		if(i % 16 == 0 && i > 0)
		 *			I("\n");
		 *		I("g_guest_info_data->g_guest_str[%d][%d]
					= 0x%02X", custom_info_temp, i,
					g_guest_info_data->g_guest_str[
					custom_info_temp][i]);
		 *	}
		 *}
		 */
	}
	/* himax_burst_enable(hx_s_ts->client, 0); */
	hx_s_core_fp._sense_on(0x01);

	kfree(flash_tmp_buffer);
	himax_guest_info_set_status(0);
	return NO_ERR;
}
#endif
/* CORE_DRIVER */

void himax_mcu_resend_cmd_func(bool suspended)
{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE)
	struct himax_ts_data *ts = hx_s_ts;
#endif
#if defined(HX_SMART_WAKEUP)
	hx_s_core_fp._set_SMWP_enable(ts->SMWP_enable, suspended);
#endif
#if defined(HX_HIGH_SENSE)
	hx_s_core_fp._set_HSEN_enable(ts->HSEN_enable, suspended);
#endif
#if defined(HX_USB_DETECT_GLOBAL)
	himax_cable_detect_func(true);
#endif
}


int hx_turn_on_mp_func(int on)
{
	int rslt = 0;
	int retry = 3;
	uint8_t tmp_data[4] = {0};
	uint8_t tmp_read[4] = {0};
	/* char *tmp_chipname = hx_s_ts->chip_name; */

	if (strcmp(HX_83102D_SERIES_PWON, hx_s_ts->chip_name) == 0) {
		if (on) {
			I("%s : Turn on MPAP mode!\n", __func__);
			hx_parse_assign_cmd(
				hx_s_ic_setup._data_ctrl_mpap_ovl_on,
				tmp_data, sizeof(tmp_data));
			do {
				hx_s_core_fp._register_write(
					hx_s_ic_setup._addr_ctrl_mpap_ovl,
					tmp_data, DATA_LEN_4);
				usleep_range(10000, 10001);
				hx_s_core_fp._register_read(
					hx_s_ic_setup._addr_ctrl_mpap_ovl,
					tmp_read, DATA_LEN_4);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__, tmp_read[2], tmp_read[1],
					tmp_read[0]);

				retry--;
			} while (((retry > 0)
			&& (tmp_read[2] != tmp_data[2]
			&& tmp_read[1] != tmp_data[1]
			&& tmp_read[0] != tmp_data[0])));
		} else {
			I("%s : Turn off MPAP mode!\n", __func__);
			hx_parse_assign_cmd(DATA_CLEAR, tmp_data,
				sizeof(tmp_data));
			do {
				hx_s_core_fp._register_write(
					hx_s_ic_setup._data_ctrl_mpap_ovl_on,
					tmp_data, DATA_LEN_4);
				usleep_range(10000, 10001);
				hx_s_core_fp._register_read(
					hx_s_ic_setup._addr_ctrl_mpap_ovl,
					tmp_read, DATA_LEN_4);

				I("%s:read2=0x%02X,read1=0x%02X,read0=0x%02X\n",
					__func__, tmp_read[2], tmp_read[1],
					tmp_read[0]);

				retry--;
			} while ((retry > 0)
			&& (tmp_read[2] != tmp_data[2]
			&& tmp_read[1] != tmp_data[1]
			&& tmp_read[0] != tmp_data[0]));
		}
	} else {
		I("%s Nothing to be done!\n", __func__);
	}

	return rslt;
}

#if defined(HX_ZERO_FLASH)
void hx_dis_rload_0f(int disable)
{
	uint8_t tmp_data[DATA_LEN_4];
	/*Diable Flash Reload*/
	hx_parse_assign_cmd(hx_s_ic_setup._data_fw_reload_dis,
			tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_chk_fw_reload,
		tmp_data, DATA_LEN_4);
}

void himax_mcu_clean_sram_0f(uint32_t addr, int write_len, int type)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int address = 0;
	int i = 0;

	uint8_t fix_data = 0x00;
	uint8_t tmp_data[MAX_I2C_TRANS_SZ] = {0};

	I("%s, Entering\n", __func__);

	total_size_temp = write_len;

	I("%s:addr = 0x%08X\n",
		__func__, addr);

	switch (type) {
	case 0:
		fix_data = 0x00;
		break;
	case 1:
		fix_data = 0xAA;
		break;
	case 2:
		fix_data = 0xBB;
		break;
	}

	for (i = 0; i < MAX_I2C_TRANS_SZ; i++)
		tmp_data[i] = fix_data;

	I("%s,  total size=%d\n", __func__, total_size_temp);

	if (total_size_temp % max_bus_size == 0)
		total_read_times = total_size_temp / max_bus_size;
	else
		total_read_times = total_size_temp / max_bus_size + 1;

	for (i = 0; i < (total_read_times); i++) {
		address = addr + i * max_bus_size;
		I("[log]write %d time start!\n", i);
		if (total_size_temp >= max_bus_size) {
			hx_s_core_fp._register_write(address,
				tmp_data, max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			I("last total_size_temp=%d\n", total_size_temp);
			hx_s_core_fp._register_write(address,
				tmp_data, total_size_temp % max_bus_size);
		}
		usleep_range(10000, 11000);
	}

	I("%s, END\n", __func__);
}

void himax_mcu_write_sram_0f(uint8_t *addr, const uint8_t *data, uint32_t len)
{
	int max_bus_size = MAX_I2C_TRANS_SZ;
	uint32_t remain_len = 0;
	uint32_t address = 0;
	uint32_t i;

	I("%s: Entering - total write size = %d\n", __func__, len);

#if defined(BUS_W_DLEN)
	max_bus_size = BUS_W_DLEN-DATA_LEN_4;
#endif

	if (strcmp(hx_s_ts->chip_name, HX_83121A_SERIES_PWON) == 0) {
		if (max_bus_size > HX4K)
			max_bus_size = HX4K;
	}

	remain_len = len;

	while (remain_len > 0) {
		address = (addr[3] << 24) |
			(addr[2] << 16) |
			(addr[1] << 8) |
			addr[0];
		i = len - remain_len;
		address += i;

		if (remain_len > max_bus_size) {
			hx_s_core_fp._register_write(address, (uint8_t *)data+i,
				max_bus_size);
			remain_len -= max_bus_size;
		} else {
			hx_s_core_fp._register_write(address, (uint8_t *)data+i,
				remain_len);
			remain_len = 0;
		}
		usleep_range(10000, 10001);
	}

	I("%s, End\n", __func__);
}

int himax_sram_write_crc_check(uint8_t *addr, const uint8_t *data, uint32_t len)
{
	int retry = 0;
	int crc = -1;

	do {
		hx_s_core_fp._write_sram_0f(addr, data, len);
		crc = hx_s_core_fp._calc_crc_by_hw(addr, len);
		retry++;
		I("%s, HW CRC %s in %d time\n", __func__,
			(crc == 0) ? "OK" : "Fail", retry);
	} while (crc != 0 && retry < 3);

	return crc;
}

static int hx_mcu_code_overlay(struct zf_info *info,
	const struct firmware *fw, int type)
{
	int ret = 0;
	int retry = 0;
	uint8_t rdata[4] = {0};
	uint8_t code_idx_t = 0;
	uint8_t code_sdata[4] = {0};

	/* ovl_idx[0] - sorting */
	/* ovl_idx[1] - gesture */
	/* ovl_idx[2] - border	*/

	code_idx_t = ovl_idx[0];
	code_sdata[0] = hx_s_ic_setup._ovl_sorting_reply;

	if (type == 0) {
#if defined(HX_SMART_WAKEUP)
		if (hx_s_ts->suspended && hx_s_ts->SMWP_enable) {
			code_idx_t = ovl_idx[1];
			code_sdata[0] = hx_s_ic_setup._ovl_gesture_reply;
		} else {
			code_idx_t = ovl_idx[2];
			code_sdata[0] = hx_s_ic_setup._ovl_border_reply;
		}
#else
		code_idx_t = ovl_idx[2];
		code_sdata[0] = hx_s_ic_setup._ovl_border_reply;
#endif
	}
	if (code_idx_t == 0 || info[code_idx_t].write_size == 0) {
		E("%s: wrong code overlay section[%d, %d]!\n", __func__,
			code_idx_t, info[code_idx_t].write_size);
		ret = FW_NOT_READY;
		goto ALOC_CFG_BUF_FAIL;
	}

	I("%s: upgrade code overlay section[%d]\n",
		__func__,
		code_idx_t);
	if (hx_s_core_fp._write_sram_0f_crc(info[code_idx_t].sram_addr,
		&fw->data[info[code_idx_t].fw_addr],
		info[code_idx_t].write_size) != 0) {
		E("%s: code overlay HW CRC FAIL\n", __func__);
		code_sdata[0] = hx_s_ic_setup._ovl_fault;
		ret = 2;
	}

	retry = 0;
	do {
		hx_s_core_fp._register_write(hx_s_ic_setup._ovl_addr_handshake,
			code_sdata,
			DATA_LEN_4);
		usleep_range(1000, 1100);
		hx_s_core_fp._register_read(hx_s_ic_setup._ovl_addr_handshake,
			rdata,
			DATA_LEN_4);
		retry++;
	} while ((code_sdata[3] != rdata[3]
	|| code_sdata[2] != rdata[2]
	|| code_sdata[1] != rdata[1]
	|| code_sdata[0] != rdata[0])
	&& retry < HIMAX_REG_RETRY_TIMES);

	if (retry >= HIMAX_REG_RETRY_TIMES) {
		E("%s: fail code rpl data = 0x%02X%02X%02X%02X\n",
		__func__, rdata[0], rdata[1], rdata[2], rdata[3]);
	}


ALOC_CFG_BUF_FAIL:
	return ret;
}
static int hx_mcu_alg_overlay(uint8_t alg_idx_t, struct zf_info *info,
	const struct firmware *fw)
{
	int ret = 0;
	int retry = 0;
	uint8_t tmp_data[DATA_LEN_4] = {0};
	uint8_t rdata[DATA_LEN_4] = {0};

	uint8_t i = 0;
	uint8_t alg_sdata[4] = {0xA5, 0x5A, 0x5A, 0xA5};



	if (alg_idx_t == 0 || info[alg_idx_t].write_size == 0) {
		E("%s: wrong alg overlay section[%d, %d]!\n", __func__,
			alg_idx_t, info[alg_idx_t].write_size);
		ret = FW_NOT_READY;
		goto ALOC_CFG_BUF_FAIL;
	}



	// clear handshaking to 0xA55A5AA5

	retry = 0;
	do {
		hx_s_core_fp._register_write(hx_s_ic_setup._ovl_addr_handshake,
			alg_sdata, DATA_LEN_4);
		usleep_range(1000, 1100);
		hx_s_core_fp._register_read(hx_s_ic_setup._ovl_addr_handshake,
			rdata, DATA_LEN_4);
	} while ((rdata[0] != alg_sdata[0]
	|| rdata[1] != alg_sdata[1]
	|| rdata[2] != alg_sdata[2]
	|| rdata[3] != alg_sdata[3])
	&& retry++ < HIMAX_REG_RETRY_TIMES);

	if (retry > HIMAX_REG_RETRY_TIMES) {
		E("%s: init handshaking data FAIL[%02X%02X%02X%02X]!!\n",
			__func__, rdata[0], rdata[1], rdata[2], rdata[3]);
	}

	alg_sdata[3] = hx_s_ic_setup._ovl_alg_reply;
	alg_sdata[2] = hx_s_ic_setup._ovl_alg_reply;
	alg_sdata[1] = hx_s_ic_setup._ovl_alg_reply;
	alg_sdata[0] = hx_s_ic_setup._ovl_alg_reply;

	hx_s_core_fp._reload_disable(0);

	/*hx_s_core_fp._power_on_init();*/
	/*Rawout Sel initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_rawout_sel,
		tmp_data, DATA_LEN_4);
	/*DSRAM func initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._assign_sorting_mode(tmp_data);
	/* reset N frame back to default for normal mode */
	hx_parse_assign_cmd(0x00000001, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_set_frame,
		tmp_data, DATA_LEN_4);
	/*FW reload done initial*/
	hx_parse_assign_cmd(DATA_CLEAR, tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_chk_fw_reload2,
		tmp_data, DATA_LEN_4);

	hx_s_core_fp._sense_on(0x00);

	retry = 0;
	do {
		usleep_range(3000, 3100);
		hx_s_core_fp._register_read(hx_s_ic_setup._ovl_addr_handshake,
			rdata, DATA_LEN_4);
	} while ((rdata[0] != hx_s_ic_setup._ovl_alg_request
	|| rdata[1] != hx_s_ic_setup._ovl_alg_request
	|| rdata[2] != hx_s_ic_setup._ovl_alg_request
	|| rdata[3] != hx_s_ic_setup._ovl_alg_request)
	&& retry++ < 30);

	if (retry > 30) {
		E("%s: fail req data = 0x%02X%02X%02X%02X\n", __func__,
			rdata[0], rdata[1], rdata[2], rdata[3]);
		/* monitor FW status for debug */
		for (i = 0; i < 10; i++) {
			usleep_range(10000, 10100);
			hx_s_core_fp._register_read(
				hx_s_ic_setup._ovl_addr_handshake,
				rdata, DATA_LEN_4);
			I("%s: req data = 0x%02X%02X%02X%02X\n",
				__func__, rdata[0], rdata[1], rdata[2],
				rdata[3]);
			hx_s_core_fp._read_FW_status();
		}
		ret = 3;
		goto BURN_OVL_FAIL;
	}

	I("%s: upgrade alg overlay section[%d]\n", __func__, alg_idx_t);

	if (hx_s_core_fp._write_sram_0f_crc(info[alg_idx_t].sram_addr,
		&fw->data[info[alg_idx_t].fw_addr],
		info[alg_idx_t].write_size) != 0) {
		E("%s: Alg Overlay HW CRC FAIL\n", __func__);
		ret = 2;
	}

	retry = 0;
	do {
		hx_s_core_fp._register_write(hx_s_ic_setup._ovl_addr_handshake,
			alg_sdata, DATA_LEN_4);
		usleep_range(1000, 1100);
		hx_s_core_fp._register_read(hx_s_ic_setup._ovl_addr_handshake,
			rdata, DATA_LEN_4);
	} while ((alg_sdata[3] != rdata[3]
	|| alg_sdata[2] != rdata[2]
	|| alg_sdata[1] != rdata[1]
	|| alg_sdata[0] != rdata[0])
	&& retry++ < HIMAX_REG_RETRY_TIMES);

	if (retry > HIMAX_REG_RETRY_TIMES) {
		E("%s: fail rpl data = 0x%02X%02X%02X%02X\n", __func__,
			rdata[0], rdata[1], rdata[2], rdata[3]);
		// maybe need to reset
	} else {
		I("%s: waiting for FW reload data", __func__);

		retry = 0;
		while (retry++ < 30) {
			hx_s_core_fp._register_read(
				hx_s_ic_setup._addr_chk_fw_reload2,
				tmp_data, DATA_LEN_4);

			/* use all 4 bytes to compare */
			if ((tmp_data[3] == 0x00 && tmp_data[2] == 0x00 &&
			tmp_data[1] == 0x72 && tmp_data[0] == 0xC0)) {
				I("%s: FW reload done\n", __func__);
					break;
			}
			I("%s: wait FW reload %d times\n", __func__, retry);
			hx_s_core_fp._read_FW_status();
			usleep_range(10000, 11000);
		}
	}

BURN_OVL_FAIL:
ALOC_CFG_BUF_FAIL:
	return ret;
}
static int himax_zf_part_info(const struct firmware *fw, int type)
{
	uint32_t table_addr = CFG_TABLE_FLASH_ADDR;
	int pnum = 0;
	int ret = 0;
	uint8_t buf[16];
	struct zf_info *info;
	// uint8_t *cfg_buf;
	uint8_t sram_min[4];
	int cfg_sz = 0;
	int cfg_crc_sw = 0;
	int cfg_crc_hw = 0;
	uint8_t i = 0;
	int i_max = 0;
	int i_min = 0;
	int i_min_fw = 0;
	uint32_t dsram_base = 0xFFFFFFFF;
	uint32_t fw_base = 0xFFFFFFFF;
	uint32_t dsram_max = 0;
	int retry = 0;
	int allovlidx = 0;
	uint8_t alg_idx_t = 0;
	uint8_t j = 0;
	bool has_code_overlay = false;

	g_has_alg_overlay = false;

	/* 1. initial check */
#if defined(HX_OPT_HW_CRC)
	hx_s_core_fp._setup_opt_hw_crc(fw);
#endif
	pnum = fw->data[table_addr + 12];
	if (pnum < 2) {
		E("%s: partition number is not correct\n", __func__);
		return FW_NOT_READY;
	}

	info = kcalloc(pnum, sizeof(struct zf_info), GFP_KERNEL);
	if (info == NULL) {
		E("%s: memory allocation fail[info]!!\n", __func__);
		return 1;
	}
	memset(info, 0, pnum * sizeof(struct zf_info));

	if (ovl_idx == NULL) {
		ovl_idx = kzalloc(hx_s_ic_setup._ovl_section_num, GFP_KERNEL);
		if (ovl_idx == NULL) {
			E("%s, ovl_idx alloc failed!\n", __func__);
			return 1;
		}
	} else {
		memset(ovl_idx, 0, hx_s_ic_setup._ovl_section_num);
	}


	/* 2. record partition information */
	memcpy(buf, &fw->data[table_addr], 16);
	memcpy(info[0].sram_addr, buf, 4);
	info[0].write_size = buf[7] << 24 | buf[6] << 16 | buf[5] << 8 | buf[4];
	info[0].fw_addr = buf[11] << 24 | buf[10] << 16 | buf[9] << 8 | buf[8];

	for (i = 1; i < pnum; i++) {
		memcpy(buf, &fw->data[i*0x10 + table_addr], 16);

		memcpy(info[i].sram_addr, buf, 4);
		info[i].write_size = buf[7] << 24 | buf[6] << 16
				| buf[5] << 8 | buf[4];
		info[i].fw_addr = buf[11] << 24 | buf[10] << 16
				| buf[9] << 8 | buf[8];
		info[i].cfg_addr = info[i].sram_addr[0];
		info[i].cfg_addr += info[i].sram_addr[1] << 8;
		info[i].cfg_addr += info[i].sram_addr[2] << 16;
		info[i].cfg_addr += info[i].sram_addr[3] << 24;

		if (info[i].cfg_addr % 4 != 0)
			info[i].cfg_addr -= (info[i].cfg_addr % 4);

		I("%s,[%d]SRAM addr=%08X, fw_addr=%08X, write_size=%d\n",
			__func__, i, info[i].cfg_addr, info[i].fw_addr,
			info[i].write_size);

		/* alg overlay section */
		if ((buf[15] == 0x77 && buf[14] == 0x88)) {
			I("%s: find alg overlay section in index %d\n",
				__func__, i);
			/* record index of alg overlay section */
			allovlidx |= 1<<i;
			alg_idx_t = i;
			g_has_alg_overlay = true;
			continue;
		}

		/* code overlay section */
		if ((buf[15] == 0x55 && buf[14] == 0x66)
		|| (buf[3] == 0x20 && buf[2] == 0x00
		&& buf[1] == 0x8C && buf[0] == 0xE0)) {
			I("%s: find code overlay section in index %d\n",
				__func__, i);
			has_code_overlay = true;
			/* record index of code overlay section */
			allovlidx |= 1<<i;
			if (buf[15] == 0x55 && buf[14] == 0x66) {
				/* current mechanism */
				j = buf[13];
				if (j < hx_s_ic_setup._ovl_section_num)
					ovl_idx[j] = i;
			} else {
				/* previous mechanism */
				if (j < hx_s_ic_setup._ovl_section_num)
					ovl_idx[j++] = i;
			}
			continue;
		}

		if (fw_base > info[i].fw_addr) {
			fw_base = info[i].fw_addr;
			i_min_fw = i;
		}
		if (dsram_base > info[i].cfg_addr) {
			dsram_base = info[i].cfg_addr;
			i_min = i;
		}
		if (dsram_max < info[i].cfg_addr) {
			dsram_max = info[i].cfg_addr;
			i_max = i;
		}
	}

	/* 3. prepare data to update */



	for (i = 0; i < DATA_LEN_4; i++)
		sram_min[i] = (info[i_min].cfg_addr>>(8*i)) & 0xFF;

	cfg_sz = (dsram_max - dsram_base) + info[i_max].write_size;
	if (cfg_sz % 16 != 0)
		cfg_sz = cfg_sz + 16 - (cfg_sz % 16);

	I("%s, cfg_sz = %d!, dsram_base = 0x%08X, dsram_max = 0x%08X\n",
		__func__, cfg_sz, dsram_base, dsram_max);

	/* config size should be smaller than DSRAM size */
	if (cfg_sz > (hx_s_ic_data->dsram_sz)) {
		E("%s: config size error[%d, %d]!!\n", __func__,
			cfg_sz, (hx_s_ic_data->dsram_sz));
		ret = LENGTH_FAIL;
		goto ALOC_CFG_BUF_FAIL;
	}

	memset(g_update_cfg_buf, 0x00,
		hx_s_ic_data->dsram_sz * sizeof(uint8_t));


	for (i = 1; i < pnum; i++) {


		/* overlay section */
		if (allovlidx & (1<<i)) {
			I("%s: skip overlay section %d\n", __func__, i);
			continue;
		}

		memcpy(&g_update_cfg_buf[info[i].cfg_addr - dsram_base],
			&fw->data[info[i].fw_addr], info[i].write_size);
	}

	/* 4. write to sram */
	/* FW entity */
	if (hx_s_core_fp._write_sram_0f_crc(info[0].sram_addr,
	&fw->data[info[0].fw_addr], info[0].write_size) != 0) {
		E("%s: HW CRC FAIL\n", __func__);
		ret = 2;
		goto BURN_SRAM_FAIL;
	} else {
		I("%s: FW entity update OK!\n", __func__);
	}

	I("%s: i_min=%d, fw_addr=0x%08x, sram_addr=0x%08X, cfg_sz=%d\n",
		__func__,
		i_min,
		info[i_min].fw_addr,
		info[i_min].cfg_addr,
		cfg_sz);
	cfg_crc_sw = hx_s_core_fp._calc_crc_by_ap(g_update_cfg_buf, 0, cfg_sz);
	I("%s: now cfg_crc_sw=0x%08X\n", __func__, cfg_crc_sw);
	do {
		hx_s_core_fp._write_sram_0f(info[i_min].sram_addr,
			g_update_cfg_buf,
			cfg_sz);
		cfg_crc_hw = hx_s_core_fp._calc_crc_by_hw(
			info[i_min].sram_addr,
			cfg_sz);
		I("%s: now cfg_crc_hw=0x%08X\n", __func__, cfg_crc_hw);
		if (cfg_crc_hw != cfg_crc_sw) {
			E("Cfg CRC FAIL,HWCRC=%X,SWCRC=%X,retry=%d\n",
				cfg_crc_hw, cfg_crc_sw, retry);
		}
	} while (cfg_crc_hw != cfg_crc_sw && retry++ < 3);

	if (retry > 3) {
		ret = 2;
		goto BURN_SRAM_FAIL;
	}

	/*write back system config*/
	if (type == 0) {
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) \
	|| defined(HX_USB_DETECT_GLOBAL)
		hx_s_core_fp._resend_cmd_func(hx_s_ts->suspended);
#endif
	}

	if (g_has_alg_overlay)
		ret = hx_mcu_alg_overlay(alg_idx_t, info, fw);
	if (has_code_overlay)
		ret = hx_mcu_code_overlay(info, fw, type);

BURN_SRAM_FAIL:
ALOC_CFG_BUF_FAIL:
	kfree(info);

	return ret;
/* ret = 1, memory allocation fail
 *     = 2, crc fail
 *     = 3, flow control error
 */
}

int himax_mcu_firmware_update_0f(const struct firmware *fw, int type)
{
	int ret = 0;
	uint8_t tmp_data[DATA_LEN_4];

	I("%s,Entering - total FW size=%d\n", __func__, (int)fw->size);

	hx_parse_assign_cmd(hx_s_ic_setup._data_system_reset,
		tmp_data, DATA_LEN_4);
	hx_s_core_fp._register_write(hx_s_ic_setup._addr_system_reset,
		tmp_data, DATA_LEN_4);

	hx_s_core_fp._sense_off(false);

	if (hx_s_core_fp._en_hw_crc != NULL)
		hx_s_core_fp._en_hw_crc(1);
	ret = himax_zf_part_info(fw, type);

	I("%s, End\n", __func__);

	return ret;
}

int hx_0f_op_file_dirly(char *file_name)
{
	const struct firmware *fw = NULL;
	int reqret = -1;
	int ret = -1;
	int type = 0; /* FW type: 0, normal; 1, MPAP */

	if (g_f_0f_updat == 1) {
		W("%s: Other thread is updating now!\n", __func__);
		return ret;
	}
	g_f_0f_updat = 1;
	I("%s: Preparing to update %s!\n", __func__, file_name);

	reqret = request_firmware(&fw, file_name, hx_s_ts->dev);
	if (reqret < 0) {
#if defined(HX_FIRMWARE_HEADER)
        if (!g_embedded_fw.data) {
		 	E("%s: g_embedded_fw is NULL\n");
	    	goto END;
	    }
		fw = &g_embedded_fw;
		I("%s: Not find FW in userspace, use embedded FW(size:%zu)\n",
			__func__, g_embedded_fw.size);
#else
		ret = reqret;
		E("%s: request firmware fail, code[%d]!!\n", __func__, ret);
		goto END;
#endif
	}

	if (strcmp(file_name, MPAP_FWNAME) == 0)
		type = 1;

	ret = hx_s_core_fp._firmware_update_0f(fw, type);

	if (reqret >= 0)
		release_firmware(fw);

	if (ret < 0)
		goto END;

if (!g_has_alg_overlay) {
	if (type == 1)
		hx_s_core_fp._turn_on_mp_func(1);
	else
		hx_s_core_fp._turn_on_mp_func(0);
	hx_s_core_fp._reload_disable(0);
	hx_s_core_fp._power_on_init();
}

END:
	g_f_0f_updat = 0;

	I("%s: END\n", __func__);
	return ret;
}

int himax_mcu_0f_excp_check(void)
{
	return NO_ERR;
}

#if defined(HX_0F_DEBUG)
void himax_mcu_read_sram_0f(const struct firmware *fw_entry,
		uint8_t *addr, int start_index, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0, j = 0;
	int not_same = 0;

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data = NULL;
	int *not_same_buff = NULL;
	uint32_t addr32 = addr[3] << 24 |
						addr[2] << 16 |
						addr[1] << 8 |
						addr[0];

	I("%s, Entering\n", __func__);

	/*hx_s_core_fp._burst_enable(1);*/

	total_size = read_len;

	total_size_temp = read_len;

#if defined(HX_SPI_OPERATION)
	if (read_len > 2048)
		max_bus_size = 2048;
	else
		max_bus_size = read_len;
#else
	if (read_len > 240)
		max_bus_size = 240;
	else
		max_bus_size = read_len;
#endif

	if (total_size % max_bus_size == 0)
		total_read_times = total_size / max_bus_size;
	else
		total_read_times = total_size / max_bus_size + 1;

	I("%s, total size=%d, bus size=%d, read time=%d\n",
		__func__,
		total_size,
		max_bus_size,
		total_read_times);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s,addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);

	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate temp_info_data\n", __func__);
		goto err_malloc_temp_info_data;
	}

	not_same_buff = kcalloc(total_size, sizeof(int), GFP_KERNEL);
	if (not_same_buff == NULL) {
		E("%s, Failed to allocate not_same_buff\n", __func__);
		goto err_malloc_not_same_buff;
	}

	for (i = 0; i < (total_read_times); i++) {
		if (i == 0)
			address = addr32;
		if (total_size_temp >= max_bus_size) {
			hx_s_core_fp._register_read(address,
				&temp_info_data[i*max_bus_size], max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			hx_s_core_fp._register_read(address,
				&temp_info_data[i*max_bus_size],
				total_size_temp % max_bus_size);
		}

		address = ((i+1) * max_bus_size);
		tmp_addr[0] = addr[0] + (uint8_t) ((address) & 0x00FF);
		if (tmp_addr[0] < addr[0])
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF) + 1;
		else
			tmp_addr[1] = addr[1]
				+ (uint8_t) ((address>>8) & 0x00FF);

		/*msleep (10);*/
	}
	I("%s,READ Start, start_index = %d\n", __func__, start_index);

	j = start_index;
	for (i = 0; i < read_len; i++, j++) {
		if (fw_entry->data[j] != temp_info_data[i]) {
			not_same++;
			not_same_buff[i] = 1;
		}

		I("0x%2.2X, ", temp_info_data[i]);

		if (i > 0 && i%16 == 15)
			pr_info("\n");

	}
	I("%s,READ END,Not Same count=%d\n", __func__, not_same);

	if (not_same != 0) {
		j = start_index;
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("bin=[%d] 0x%2.2X\n", i, fw_entry->data[j]);
		}
		for (i = 0; i < read_len; i++, j++) {
			if (not_same_buff[i] == 1)
				I("sram=[%d] 0x%2.2X\n", i, temp_info_data[i]);
		}
	}
	I("%s,READ END, Not Same count=%d\n", __func__, not_same);

	kfree(not_same_buff);
err_malloc_not_same_buff:
	kfree(temp_info_data);
err_malloc_temp_info_data:
	return;
}

void himax_mcu_read_all_sram(uint8_t *addr, int read_len)
{
	int total_read_times = 0;
	int max_bus_size = MAX_I2C_TRANS_SZ;
	int total_size_temp = 0;
	int total_size = 0;
	int address = 0;
	int i = 0;
	/* struct file *fn; */
	/* struct filename *vts_name;	*/

	uint8_t tmp_addr[4];
	uint8_t *temp_info_data;

	I("%s, Entering\n", __func__);

	/*hx_s_core_fp._burst_enable(1);*/

	total_size = read_len;

	total_size_temp = read_len;

	if (total_size % max_bus_size == 0)
		total_read_times = total_size / max_bus_size;
	else
		total_read_times = total_size / max_bus_size + 1;

	I("%s, total size=%d\n", __func__, total_size);

	tmp_addr[3] = addr[3];
	tmp_addr[2] = addr[2];
	tmp_addr[1] = addr[1];
	tmp_addr[0] = addr[0];
	I("%s:addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);

	temp_info_data = kcalloc(total_size, sizeof(uint8_t), GFP_KERNEL);
	if (temp_info_data == NULL) {
		E("%s, Failed to allocate temp_info_data\n", __func__);
		return;
	}

	address = tmp_addr[3] << 24 |
				tmp_addr[2] << 16 |
				tmp_addr[1] << 8 |
				tmp_addr[0];

	for (i = 0; i < (total_read_times); i++) {
		if (total_size_temp >= max_bus_size) {
			hx_s_core_fp._register_read(address,
				&temp_info_data[i*max_bus_size], max_bus_size);
			total_size_temp = total_size_temp - max_bus_size;
		} else {
			hx_s_core_fp._register_read(address,
				&temp_info_data[i*max_bus_size],
				total_size_temp % max_bus_size);
		}

		address += max_bus_size;

		/*msleep (10);*/
	}
	I("%s,addr[3]=0x%2X,addr[2]=0x%2X,addr[1]=0x%2X,addr[0]=0x%2X\n",
		__func__,
		tmp_addr[3],
		tmp_addr[2],
		tmp_addr[1],
		tmp_addr[0]);
	/*for(i = 0;i<read_len;i++)
	 *{
	 *	I("0x%2.2X, ", temp_info_data[i]);
	 *
	 *	if (i > 0 && i%16 == 15)
	 *		printk("\n");
	 *}
	 */

	/* need modify
	 * I("Now Write File start!\n");
	 * vts_name = kp_getname_kernel("/sdcard/dump_dsram.txt");
	 * fn = kp_file_open_name(vts_name, O_CREAT | O_WRONLY, 0);
	 * if (!IS_ERR (fn)) {
	 * I("%s create file and ready to write\n", __func__);
	 * fn->f_op->write(fn, temp_info_data, read_len*sizeof (uint8_t),
	 *	&fn->f_pos);
	 * filp_close (fn, NULL);
	 * }
	 * I("Now Write File End!\n");
	 */

	kfree(temp_info_data);

	I("%s, END\n", __func__);

}

#endif


/*
 *#if defined(HX_CODE_OVERLAY)
 *int himax_mcu_0f_overlay(int ovl_type, int mode)
 *{
 *return NO_ERR;
 *}
 *#endif
 */

#endif
