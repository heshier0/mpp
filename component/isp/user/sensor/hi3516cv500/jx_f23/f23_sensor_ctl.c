/******************************************************************************

  Copyright (C), 2016, Hisilicon Tech. Co., Ltd.

 ******************************************************************************
  File Name		: soi_sensor_sensor_ctl.c
  Version		: Initial Draft
  Author		: Hisilicon multimedia software group
  Created		: 2019/02/12
  Description	:
  History		:
  1.Date		: 2019/02/15
	Author		:
******************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include <hi_math.h>
#include "hi_comm_video.h"
#include "hi_sns_ctrl.h"									//hi3516ev200

#ifdef HI_GPIO_I2C
#include "gpioi2c_ex.h"
#else
#include "hi_i2c.h"
#endif


//#define	_F23_NOISE_OPT									//min@20180302


const unsigned char	soi_sensor_i2c_addr	 = 0x80;			//Sensor I2C Address
const unsigned int	soi_sensor_addr_byte = 1;
const unsigned int	soi_sensor_data_byte = 1;
static int	g_fd[ISP_MAX_PIPE_NUM] = {[0 ... (ISP_MAX_PIPE_NUM - 1)] = -1};

//# extern WDR_MODE_E	genSensorMode;
//# extern HI_U8		gu8SensorImageMode;
//# extern HI_BOOL		bSensorInit;
extern ISP_SNS_STATE_S		*g_pastSoiSensor[ISP_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U	g_aunSoiSensorBusInfo[];

extern HI_U8		Reg0D;				//min@20190109 add

int soi_sensor_i2c_init(VI_PIPE ViPipe)
{
	char	acDevFile[16] = {0};
	HI_U8	u8DevNum;
	
	if( g_fd[ViPipe] >= 0 ) {
		return HI_SUCCESS;
	}
	
#ifdef HI_GPIO_I2C
	int	ret;
	
	g_fd[ViPipe] = open( "/dev/gpioi2c_ex", O_RDONLY, S_IRUSR );
	if( g_fd[ViPipe] < 0 ) {
		ISP_TRACE(HI_DBG_ERR, "Open gpioi2c_ex error!\n" );
		return HI_FAILURE;
	}
#else
	int	ret;
	
	u8DevNum = g_aunSoiSensorBusInfo[ViPipe].s8I2cDev;
	
	printf("!!! jxf23:i2c-%d\n", u8DevNum);
	
	snprintf( acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum );
	
	g_fd[ViPipe] = open( acDevFile, O_RDWR, S_IRUSR | S_IWUSR );
	if( g_fd[ViPipe] < 0 ) {
		ISP_TRACE(HI_DBG_ERR, "Open /dev/hi_i2c_drv-%u error!\n", u8DevNum );
		return HI_FAILURE;
	}
	
	ret = ioctl( g_fd[ViPipe], I2C_SLAVE_FORCE, (soi_sensor_i2c_addr>>1) );
	if( ret < 0 ) {
		ISP_TRACE(HI_DBG_ERR, "I2C_SLAVE_FORCE error!\n" );
		close( g_fd[ViPipe] );
		g_fd[ViPipe] = -1;
		return ret;
	}
#endif
	
	return HI_SUCCESS;
}

int soi_sensor_i2c_exit(VI_PIPE ViPipe)
{
	if( g_fd[ViPipe] >= 0 ) {
		close( g_fd[ViPipe] );
		g_fd[ViPipe] = -1;
		return HI_SUCCESS;
	}
	return HI_FAILURE;
}

int soi_sensor_read_register(VI_PIPE ViPipe, int addr)
{
	// TODO:
	
	return HI_SUCCESS;
	//# return i2c_read( 0, soi_sensor_i2c_addr, addr, addr, 1, 1, 1 );
}

int soi_sensor_write_register(VI_PIPE ViPipe, int addr, int data)
{
	if( 0 > g_fd[ViPipe] ) {
		return HI_SUCCESS;
	}
	
#ifdef HI_GPIO_I2C
	i2c_data.dev_addr		= soi_sensor_i2c_addr;
	i2c_data.reg_addr		= addr;
	i2c_data.addr_byte_num	= soi_sensor_addr_byte;
	i2c_data.data			= data;
	i2c_data.data_byte_num	= soi_sensor_data_byte;
	
	ret = ioctl( g_fd[ViPipe], GPIO_I2C_WRITE, &i2c_data );
	if( ret ) {
		ISP_TRACE(HI_DBG_ERR, "GPIO-I2C write faild!\n" );
		return ret;
	}
#else
	int		idx = 0;
	int		ret;
	char	buf[8] = {0};
	
	if( soi_sensor_addr_byte == 2 ) {
		buf[idx] = (addr >> 8) & 0xff;
		idx++;
		buf[idx] = addr & 0xff;
		idx++;
	}
	else {
		buf[idx] = addr & 0xff;
		idx++;
	}
	
	if( soi_sensor_data_byte == 2 ) {
		buf[idx] = (data >> 8) & 0xff;
		idx++;
		buf[idx] = data & 0xff;
		idx++;
	}
	else {
		buf[idx] = data & 0xff;
		idx++;
	}
	
	ret = write( g_fd[ViPipe], buf, soi_sensor_addr_byte + soi_sensor_data_byte );
	if( ret < 0 ) {
		ISP_TRACE(HI_DBG_ERR, "I2C_WRITE error!\n" );
		return HI_FAILURE;
	}
#endif
	return HI_SUCCESS;
}


static void delay_ms(int ms)
{
	usleep( ms * 1000 );
}

void soi_sensor_prog(VI_PIPE ViPipe, int *rom)
{
	int i = 0;
	
	while( 1 ) {
		int lookup = rom[i++];
		int addr = (lookup >> 16) & 0xFFFF;
		int data = lookup & 0xFFFF;
		
		if( addr == 0xFFFE ) {
			delay_ms( data );
		}
		else if( addr == 0xFFFF ) {
			return;
		}
		else {
			soi_sensor_write_register( ViPipe, addr, data );
		}
	}
}

void soi_sensor_standby(VI_PIPE ViPipe)
{
	// TODO:
}

void soi_sensor_restart(VI_PIPE ViPipe)
{
	// TODO:
}

#define		SOI_SENSOR_1080P_30FPS_LINEAR_MODE	(1)
#define		SOI_SENSOR_1080P_30FPS_WDR_MODE		(2)

void soi_sensor_linear_1080p30_init(VI_PIPE ViPipe);
void soi_sensor_2wdr1_1080p30_init(VI_PIPE ViPipe);

void soi_sensor_init(VI_PIPE ViPipe)
{
	//HI_U32	i;
	//HI_BOOL	bInit;
	//HI_U8		u8ImgMode;
	
	//bInit		= g_pastSoiSensor[ViPipe]->bInit;
	//u8ImgMode	= g_pastSoiSensor[ViPipe]->u8ImgMode;
	
	printf( "[JXF23] Date: %s\n", __DATE__ );
	printf( "[JXF23] Time: %s\n", __TIME__ );
	printf( ">> soi_sensor_init()\n" );
	
	soi_sensor_i2c_init( ViPipe );
	
	/* When sensor first init, config all registers */
	//# if( HI_FALSE == bSensorInit )
//	if( HI_FALSE == g_pastSoiSensor[ViPipe]->bInit )
	{
//		if( WDR_MODE_2To1_LINE == pstSnsState->enWDRMode )
		{
//			soi_sensor_2wdr1_1080p30_init( ViPipe );
//			g_pastSoiSensor[ViPipe]->bInit = HI_TRUE;
			//# bSensorInit = HI_TRUE;
		}
//		else
//		{
			soi_sensor_linear_1080p30_init( ViPipe );
			g_pastSoiSensor[ViPipe]->bInit = HI_TRUE;
			//# bSensorInit = HI_TRUE;
//		}
	}
	/* When sensor switch mode(linear<->WDR or resolution), config different registers(if possible) */
//	else
//	{
//		if( WDR_MODE_2To1_LINE == pstSnsState->enWDRMode )
//		{
//			soi_sensor_2wdr1_1080p30_init( ViPipe );
//		}
//		else
//		{
//			soi_sensor_linear_1080p30_init( ViPipe );
//		}
//	}
	
	printf( "<< soi_sensor_init()\n" );
}

void soi_sensor_exit(VI_PIPE ViPipe)
{
	soi_sensor_i2c_exit( ViPipe );
}

void soi_sensor_linear_1080p30_init(VI_PIPE ViPipe)
{
	printf( ">> soi_sensor_linear_1080p30_init()\n" );
	
	delay_ms( 10 );
	
    //F23#79#20190610  2560*1125
	soi_sensor_write_register( ViPipe, 0x0E, 0x11 );
	soi_sensor_write_register( ViPipe, 0x0F, 0x14 );
	soi_sensor_write_register( ViPipe, 0x10, 0x40 );
	soi_sensor_write_register( ViPipe, 0x11, 0x80 );
	soi_sensor_write_register( ViPipe, 0x48, 0x05 );
	soi_sensor_write_register( ViPipe, 0x96, 0xAA );
	soi_sensor_write_register( ViPipe, 0x94, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x97, 0x8D );
	soi_sensor_write_register( ViPipe, 0x96, 0x00 );
	soi_sensor_write_register( ViPipe, 0x12, 0x40 );
	soi_sensor_write_register( ViPipe, 0x48, 0x8A );
	soi_sensor_write_register( ViPipe, 0x48, 0x0A );
	soi_sensor_write_register( ViPipe, 0x0E, 0x11 );
	soi_sensor_write_register( ViPipe, 0x0F, 0x14 );
	soi_sensor_write_register( ViPipe, 0x10, 0x24 );
	soi_sensor_write_register( ViPipe, 0x11, 0x80 );
	soi_sensor_write_register( ViPipe, 0x0D, 0xA0 );
	soi_sensor_write_register( ViPipe, 0x5F, 0x41 );
	soi_sensor_write_register( ViPipe, 0x60, 0x20 );
	soi_sensor_write_register( ViPipe, 0x58, 0x12 );
	soi_sensor_write_register( ViPipe, 0x57, 0x60 );
	soi_sensor_write_register( ViPipe, 0x9D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x20, 0x00 );
	soi_sensor_write_register( ViPipe, 0x21, 0x05 );
	soi_sensor_write_register( ViPipe, 0x22, 0x65 );
	soi_sensor_write_register( ViPipe, 0x23, 0x04 );
	soi_sensor_write_register( ViPipe, 0x24, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x25, 0x38 );
	soi_sensor_write_register( ViPipe, 0x26, 0x43 );
	soi_sensor_write_register( ViPipe, 0x27, 0xC3 );
	soi_sensor_write_register( ViPipe, 0x28, 0x19 );
	soi_sensor_write_register( ViPipe, 0x29, 0x04 );
	soi_sensor_write_register( ViPipe, 0x2C, 0x00 );
	soi_sensor_write_register( ViPipe, 0x2D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x2E, 0x18 );
	soi_sensor_write_register( ViPipe, 0x2F, 0x44 );
	soi_sensor_write_register( ViPipe, 0x41, 0xC9 );
	soi_sensor_write_register( ViPipe, 0x42, 0x13 );
	soi_sensor_write_register( ViPipe, 0x46, 0x00 );
	soi_sensor_write_register( ViPipe, 0x76, 0x60 );
	soi_sensor_write_register( ViPipe, 0x77, 0x09 );
	soi_sensor_write_register( ViPipe, 0x1D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x1E, 0x04 );
	soi_sensor_write_register( ViPipe, 0x6C, 0x40 );
	soi_sensor_write_register( ViPipe, 0x68, 0x00 );
	soi_sensor_write_register( ViPipe, 0x6E, 0x2C );
	soi_sensor_write_register( ViPipe, 0x70, 0x6C );
	soi_sensor_write_register( ViPipe, 0x71, 0x6D );
	soi_sensor_write_register( ViPipe, 0x72, 0x6A );
	soi_sensor_write_register( ViPipe, 0x73, 0x36 );
	soi_sensor_write_register( ViPipe, 0x74, 0x02 );
	soi_sensor_write_register( ViPipe, 0x78, 0x9E );
	soi_sensor_write_register( ViPipe, 0x89, 0x01 );
	soi_sensor_write_register( ViPipe, 0x2A, 0xB1 );
	soi_sensor_write_register( ViPipe, 0x2B, 0x24 );
	soi_sensor_write_register( ViPipe, 0x31, 0x08 );
	soi_sensor_write_register( ViPipe, 0x32, 0x4F );
	soi_sensor_write_register( ViPipe, 0x33, 0x20 );
	soi_sensor_write_register( ViPipe, 0x34, 0x5E );
	soi_sensor_write_register( ViPipe, 0x35, 0x5E );
	soi_sensor_write_register( ViPipe, 0x3A, 0xAF );
	soi_sensor_write_register( ViPipe, 0x56, 0x32 );
	soi_sensor_write_register( ViPipe, 0x59, 0xBF );
	soi_sensor_write_register( ViPipe, 0x5A, 0x04 );
	soi_sensor_write_register( ViPipe, 0x85, 0x5A );
	soi_sensor_write_register( ViPipe, 0x8A, 0x04 );
	soi_sensor_write_register( ViPipe, 0x8F, 0x90 );
	soi_sensor_write_register( ViPipe, 0x91, 0x13 );
	soi_sensor_write_register( ViPipe, 0x5B, 0xA0 );
	soi_sensor_write_register( ViPipe, 0x5C, 0xF0 );
	soi_sensor_write_register( ViPipe, 0x5D, 0xFC );
	soi_sensor_write_register( ViPipe, 0x5E, 0x1F );
	soi_sensor_write_register( ViPipe, 0x62, 0x04 );
	soi_sensor_write_register( ViPipe, 0x63, 0x0F );
	soi_sensor_write_register( ViPipe, 0x64, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x66, 0x44 );
	soi_sensor_write_register( ViPipe, 0x67, 0x73 );
	soi_sensor_write_register( ViPipe, 0x69, 0x7C );
	soi_sensor_write_register( ViPipe, 0x6A, 0x28 );
    soi_sensor_write_register( ViPipe, 0x7A, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x4A, 0x05 );
	soi_sensor_write_register( ViPipe, 0x7E, 0xCD );
	soi_sensor_write_register( ViPipe, 0x49, 0x10 );
	soi_sensor_write_register( ViPipe, 0x50, 0x02 );
	soi_sensor_write_register( ViPipe, 0x7B, 0x4A );
	soi_sensor_write_register( ViPipe, 0x7C, 0x0C );
	soi_sensor_write_register( ViPipe, 0x7F, 0x57 );
	soi_sensor_write_register( ViPipe, 0x90, 0x00 );
	soi_sensor_write_register( ViPipe, 0x8E, 0x00 );
	soi_sensor_write_register( ViPipe, 0x8C, 0xFF );
    soi_sensor_write_register( ViPipe, 0x8D, 0xC7 );
	soi_sensor_write_register( ViPipe, 0x8B, 0x01 );
	soi_sensor_write_register( ViPipe, 0x0C, 0x40 );
	soi_sensor_write_register( ViPipe, 0x65, 0x02 );
	soi_sensor_write_register( ViPipe, 0x80, 0x1A );
	soi_sensor_write_register( ViPipe, 0x81, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x19, 0x20 );
	soi_sensor_write_register( ViPipe, 0x99, 0x0F );
	soi_sensor_write_register( ViPipe, 0x9B, 0x0F );
	soi_sensor_write_register( ViPipe, 0x12, 0x00 );
	soi_sensor_write_register( ViPipe, 0x48, 0x8A );
	soi_sensor_write_register( ViPipe, 0x48, 0x0A );
	
	printf( "<< soi_sensor_linear_1080p30_init()\n" );
}

void soi_sensor_2wdr1_1080p30_init(VI_PIPE ViPipe)
{
	printf( ">> soi_sensor_2wdr1_1080p30_init()\n" );
	
	delay_ms( 10 );
	
	soi_sensor_write_register( ViPipe, 0x0E, 0x11 );
	soi_sensor_write_register( ViPipe, 0x0F, 0x14 );
	soi_sensor_write_register( ViPipe, 0x10, 0x40 );
	soi_sensor_write_register( ViPipe, 0x11, 0x80 );
	soi_sensor_write_register( ViPipe, 0x48, 0x05 );
	soi_sensor_write_register( ViPipe, 0x96, 0xAA );
	soi_sensor_write_register( ViPipe, 0x94, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x97, 0x8D );
	soi_sensor_write_register( ViPipe, 0x96, 0x00 );
	soi_sensor_write_register( ViPipe, 0x12, 0x40 );
	soi_sensor_write_register( ViPipe, 0x48, 0x8A );
	soi_sensor_write_register( ViPipe, 0x48, 0x0A );
	soi_sensor_write_register( ViPipe, 0x0E, 0x11 );
	soi_sensor_write_register( ViPipe, 0x0F, 0x14 );
	soi_sensor_write_register( ViPipe, 0x10, 0x24 );
	soi_sensor_write_register( ViPipe, 0x11, 0x80 );
	soi_sensor_write_register( ViPipe, 0x0D, 0xA0 );
	soi_sensor_write_register( ViPipe, 0x5F, 0x41 );
	soi_sensor_write_register( ViPipe, 0x60, 0x20 );
	soi_sensor_write_register( ViPipe, 0x58, 0x12 );
	soi_sensor_write_register( ViPipe, 0x57, 0x60 );
	soi_sensor_write_register( ViPipe, 0x9D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x20, 0x00 );
	soi_sensor_write_register( ViPipe, 0x21, 0x05 );
	soi_sensor_write_register( ViPipe, 0x22, 0x65 );
	soi_sensor_write_register( ViPipe, 0x23, 0x04 );
	soi_sensor_write_register( ViPipe, 0x24, 0xC0 );
	soi_sensor_write_register( ViPipe, 0x25, 0x38 );
	soi_sensor_write_register( ViPipe, 0x26, 0x43 );
	soi_sensor_write_register( ViPipe, 0x27, 0xC3 );
	soi_sensor_write_register( ViPipe, 0x28, 0x19 );
	soi_sensor_write_register( ViPipe, 0x29, 0x04 );
	soi_sensor_write_register( ViPipe, 0x2C, 0x00 );
	soi_sensor_write_register( ViPipe, 0x2D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x2E, 0x18 );
	soi_sensor_write_register( ViPipe, 0x2F, 0x44 );
	soi_sensor_write_register( ViPipe, 0x41, 0xC9 );
	soi_sensor_write_register( ViPipe, 0x42, 0x13 );
	soi_sensor_write_register( ViPipe, 0x46, 0x00 );
	soi_sensor_write_register( ViPipe, 0x76, 0x60 );
	soi_sensor_write_register( ViPipe, 0x77, 0x09 );
	soi_sensor_write_register( ViPipe, 0x1D, 0x00 );
	soi_sensor_write_register( ViPipe, 0x1E, 0x04 );
	soi_sensor_write_register( ViPipe, 0x6C, 0x40 );
	soi_sensor_write_register( ViPipe, 0x68, 0x00 );
	soi_sensor_write_register( ViPipe, 0x6E, 0x2C );
	soi_sensor_write_register( ViPipe, 0x70, 0x6C );
	soi_sensor_write_register( ViPipe, 0x71, 0x6D );
	soi_sensor_write_register( ViPipe, 0x72, 0x6A );
	soi_sensor_write_register( ViPipe, 0x73, 0x36 );
	soi_sensor_write_register( ViPipe, 0x74, 0x02 );
	soi_sensor_write_register( ViPipe, 0x78, 0x9E );
	soi_sensor_write_register( ViPipe, 0x89, 0x01 );
	soi_sensor_write_register( ViPipe, 0x2A, 0xB1 );
	soi_sensor_write_register( ViPipe, 0x2B, 0x24 );
	soi_sensor_write_register( ViPipe, 0x31, 0x08 );
	soi_sensor_write_register( ViPipe, 0x32, 0x4F );
	soi_sensor_write_register( ViPipe, 0x33, 0x20 );
	soi_sensor_write_register( ViPipe, 0x34, 0x5E );
    soi_sensor_write_register( ViPipe, 0x35, 0x5E );
    soi_sensor_write_register( ViPipe, 0x3A, 0xAF );
    soi_sensor_write_register( ViPipe, 0x56, 0x32 );
    soi_sensor_write_register( ViPipe, 0x59, 0xBF );
    soi_sensor_write_register( ViPipe, 0x5A, 0x04 );
    soi_sensor_write_register( ViPipe, 0x85, 0x5A );
    soi_sensor_write_register( ViPipe, 0x8A, 0x04 );
    soi_sensor_write_register( ViPipe, 0x8F, 0x90 );
    soi_sensor_write_register( ViPipe, 0x91, 0x13 );
    soi_sensor_write_register( ViPipe, 0x5B, 0xA0 );
    soi_sensor_write_register( ViPipe, 0x5C, 0xF0 );
    soi_sensor_write_register( ViPipe, 0x5D, 0xFC );
    soi_sensor_write_register( ViPipe, 0x5E, 0x1F );
    soi_sensor_write_register( ViPipe, 0x62, 0x04 );
    soi_sensor_write_register( ViPipe, 0x63, 0x0F );
    soi_sensor_write_register( ViPipe, 0x64, 0xC0 );
    soi_sensor_write_register( ViPipe, 0x66, 0x44 );
    soi_sensor_write_register( ViPipe, 0x67, 0x73 );
    soi_sensor_write_register( ViPipe, 0x69, 0x7C );
    soi_sensor_write_register( ViPipe, 0x6A, 0x28 );
    soi_sensor_write_register( ViPipe, 0x7A, 0xC0 );
    soi_sensor_write_register( ViPipe, 0x4A, 0x05 );
    soi_sensor_write_register( ViPipe, 0x7E, 0xCD );
    soi_sensor_write_register( ViPipe, 0x49, 0x10 );
    soi_sensor_write_register( ViPipe, 0x50, 0x02 );
    soi_sensor_write_register( ViPipe, 0x7B, 0x4A );
    soi_sensor_write_register( ViPipe, 0x7C, 0x0C );
    soi_sensor_write_register( ViPipe, 0x7F, 0x57 );
    soi_sensor_write_register( ViPipe, 0x90, 0x00 );
    soi_sensor_write_register( ViPipe, 0x8E, 0x00 );
    soi_sensor_write_register( ViPipe, 0x8C, 0xFF );
    soi_sensor_write_register( ViPipe, 0x8D, 0xC7 );
    soi_sensor_write_register( ViPipe, 0x8B, 0x01 );
    soi_sensor_write_register( ViPipe, 0x0C, 0x40 );
    soi_sensor_write_register( ViPipe, 0x65, 0x02 );
    soi_sensor_write_register( ViPipe, 0x80, 0x1A );
    soi_sensor_write_register( ViPipe, 0x81, 0xC0 );
    soi_sensor_write_register( ViPipe, 0x19, 0x20 );
    soi_sensor_write_register( ViPipe, 0x99, 0x0F );
    soi_sensor_write_register( ViPipe, 0x9B, 0x0F );
    soi_sensor_write_register( ViPipe, 0x12, 0x00 );
    soi_sensor_write_register( ViPipe, 0x48, 0x8A );
    soi_sensor_write_register( ViPipe, 0x48, 0x0A );

    printf("=========================================================================\n");
    printf("===soi_sensor_2wdr1_1080p30_init success!===\n");
    printf("=========================================================================\n");
	
	printf( "<< soi_sensor_2wdr1_1080p30_init()\n" );
}

