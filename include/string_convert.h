///////////////////////////////////////////////////////////////////////////////
//               Mountain View Silicon Tech. Inc.
//  Copyright 2012, Mountain View Silicon Tech. Inc., Shanghai, China
//                       All rights reserved.
//  Filename: string_convert.h
//  maintainer: Halley
///////////////////////////////////////////////////////////////////////////////

/**
* @addtogroup 字符编码
* @{
* @defgroup string_convert string_convert
* @{
*/

#ifndef __STRING_CONVERT_H__
#define __STRING_CONVERT_H__

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus


#define UNICODE_TO_GBK     0x0
#define UNICODE_BIG_TO_GBK 0x1
#define UTF8_TO_GBK        0x2
// 用户可在下面继续扩展其他类型的转换类型
#define GBK_TO_UNICODE 0x3

#define UNKNOWN_CONVERT 0xFF



#ifdef __cplusplus
}
#endif//__cplusplus

#endif/*__STRING_CONVERT_H_*/

/**
 * @}
 * @}
 */


