#ifndef _SKEIN_IV_H_
#define _SKEIN_IV_H_

#include "skein.h"    /* get Skein macros and types */

/*
***************** Pre-computed Skein IVs *******************
**
** NOTE: these values are not "magic" constants, but
** are generated using the Threefish block function.
** They are pre-computed here only for speed; i.e., to
** avoid the need for a Threefish call during Init().
**
** The IV for any fixed hash length may be pre-computed.
** Only the most common values are included here.
**
************************************************************
**/

#define MK_64 SKEIN_MK_64

#ifndef ONLY_SKEIN_256_256
/* blkSize =  256 bits. hashSize =  128 bits */
const u64b_t SKEIN_256_IV_128[] =
    {
    MK_64(0x302F7EA2,0x3D7FE2E1),
    MK_64(0xADE4683A,0x6913752B),
    MK_64(0x975CFABE,0xF208AB0A),
    MK_64(0x2AF4BA95,0xF831F55B)
    };

/* blkSize =  256 bits. hashSize =  160 bits */
const u64b_t SKEIN_256_IV_160[] =
    {
    MK_64(0xA38A0D80,0xA3687723),
    MK_64(0xB73CDB6A,0x5963FFC9),
    MK_64(0x9633E8EA,0x07A1B447),
    MK_64(0xCA0ED09E,0xC9529C22)
    };

/* blkSize =  256 bits. hashSize =  224 bits */
const u64b_t SKEIN_256_IV_224[] =
    {
    MK_64(0xB8092969,0x9AE0F431),
    MK_64(0xD340DC14,0xA06929DC),
    MK_64(0xAE866594,0xBDE4DC5A),
    MK_64(0x339767C2,0x5A60EA1D)
    };
#endif
/* blkSize =  256 bits. hashSize =  256 bits */
const u64b_t SKEIN_256_IV_256[] =
    {
    MK_64(0x38851268,0x0E660046),
    MK_64(0x4B72D5DE,0xC5A8FF01),
    MK_64(0x281A9298,0xCA5EB3A5),
    MK_64(0x54CA5249,0xF46070C4)
    };

#ifndef ONLY_SKEIN_256

/* blkSize =  512 bits. hashSize =  128 bits */
const u64b_t SKEIN_512_IV_128[] =
    {
    MK_64(0x477DF9EF,0xAFC4F08A),
    MK_64(0x7A64D342,0x33660E14),
    MK_64(0x71653C44,0xCEBC89C5),
    MK_64(0x63D2A36D,0x65B0AB91),
    MK_64(0x52B93FB0,0x9782EA89),
    MK_64(0x20F36980,0x8B960829),
    MK_64(0xE8DF80FB,0x30303B9B),
    MK_64(0xB89D3902,0x1A476D1F)
    };

/* blkSize =  512 bits. hashSize =  160 bits */
const u64b_t SKEIN_512_IV_160[] =
    {
    MK_64(0x0045FA2C,0xAD913A2C),
    MK_64(0xF45C9A76,0xBF75CE81),
    MK_64(0x0ED758A9,0x3D1F266B),
    MK_64(0xC0E65E85,0x1EDCD67A),
    MK_64(0x1E024D51,0xF5E7583E),
    MK_64(0xA271F855,0x4E52B0E1),
    MK_64(0x5292867D,0x8AC674F9),
    MK_64(0xADA325FA,0x60C3B226)
    };

/* blkSize =  512 bits. hashSize =  224 bits */
const u64b_t SKEIN_512_IV_224[] =
    {
    MK_64(0xF2DAA169,0x8216CC98),
    MK_64(0x00E06A48,0x8983AE05),
    MK_64(0xC080CEA9,0x5948958F),
    MK_64(0x2A8F314B,0x57F4ADD1),
    MK_64(0xBCD06591,0x360A405A),
    MK_64(0xF81A11A1,0x02D91F70),
    MK_64(0x85C6FFA5,0x4810A739),
    MK_64(0x1E07AFE0,0x1802CE74)
    };

/* blkSize =  512 bits. hashSize =  256 bits */
const u64b_t SKEIN_512_IV_256[] =
    {
    MK_64(0x88C07F38,0xD4F95AD4),
    MK_64(0x3DF0D33A,0x8610E240),
    MK_64(0x3E243F6E,0xDB6FAC74),
    MK_64(0xBAC4F4CD,0xD7A90A24),
    MK_64(0xDF90FD1F,0xDEEEBA04),
    MK_64(0xA4F5796B,0xDB7FDDA8),
    MK_64(0xDA182FD2,0x964BC923),
    MK_64(0x55F76677,0xEF6961F9)
    };

/* blkSize =  512 bits. hashSize =  384 bits */
const u64b_t SKEIN_512_IV_384[] =
    {
    MK_64(0xE5BF4D02,0xBA62494C),
    MK_64(0x7AA1EABC,0xC3E6FC68),
    MK_64(0xBBE5FC26,0xE1038C5A),
    MK_64(0x53C9903E,0x8F88E9FA),
    MK_64(0xF30D8DDD,0xFB940C83),
    MK_64(0x500FDA3C,0x4865ABEC),
    MK_64(0x2226C67F,0x745BC5E7),
    MK_64(0x015DA800,0x77C639F7)
    };

/* blkSize =  512 bits. hashSize =  512 bits */
const u64b_t SKEIN_512_IV_512[] =
    {
    MK_64(0xA8D47980,0x544A6E32),
    MK_64(0x84751153,0x3E9B1A8A),
    MK_64(0x6FAEE870,0xD8E81A00),
    MK_64(0x58B0D9D6,0xCB557F92),
    MK_64(0x9BBC0051,0xDAC1D4E9),
    MK_64(0xB744E2B1,0xD189E7CA),
    MK_64(0x979350FA,0x709C5EF3),
    MK_64(0x0350125A,0x92067BCD)
    };

/* blkSize = 1024 bits. hashSize =  384 bits */
const u64b_t SKEIN1024_IV_384[] =
    {
    MK_64(0x7600B2E3,0x9FC73E48),
    MK_64(0x7A4543BB,0xECCD60E4),
    MK_64(0x8AB879D6,0x2F53E192),
    MK_64(0x14847919,0xA7F0AC6E),
    MK_64(0x4F774735,0xAA99CB7F),
    MK_64(0x607CF3C2,0x41760EE1),
    MK_64(0xC0BF6D7B,0xFF9F27DB),
    MK_64(0x7D321485,0x99342254),
    MK_64(0xE7231BB0,0xCDF9DD49),
    MK_64(0x641DE8E6,0x464DB3F4),
    MK_64(0x05613046,0xA01CF005),
    MK_64(0x7347EE0B,0xB09E8BCC),
    MK_64(0x5103A256,0x161A26FF),
    MK_64(0x8161EAC4,0x3A1176C2),
    MK_64(0xB9607373,0xCF92C2CC),
    MK_64(0xFDE8D4AD,0xD376300D)
    };

/* blkSize = 1024 bits. hashSize =  512 bits */
const u64b_t SKEIN1024_IV_512[] =
    {
    MK_64(0x8CF63BE5,0xE1EDF4B7),
    MK_64(0x256FD425,0xCBDE61EB),
    MK_64(0x497B412D,0xEBA3EF9D),
    MK_64(0x3CBD412A,0xD8293FBA),
    MK_64(0xD5AE34D6,0xF26F646E),
    MK_64(0x72879C01,0x0DA17B79),
    MK_64(0x61BD8F18,0x05AFF621),
    MK_64(0x75CB3C94,0x9CE0E463),
    MK_64(0xAF27329D,0x2CD71E37),
    MK_64(0x7DB5EC5E,0x1141CE9F),
    MK_64(0x76484C13,0x20CAB67B),
    MK_64(0x57EB52A6,0x561BE8C5),
    MK_64(0x51161125,0xE681412D),
    MK_64(0xF510D937,0x5439A9BC),
    MK_64(0xD18AF77C,0xFC425E3C),
    MK_64(0xEB05160C,0x3FEBB037)
    };

/* blkSize = 1024 bits. hashSize = 1024 bits */
const u64b_t SKEIN1024_IV_1024[] =
    {
    MK_64(0x5A4352BE,0x62092156),
    MK_64(0x5F6E8B1A,0x72F001CA),
    MK_64(0xFFCBFE9C,0xA1A2CE26),
    MK_64(0x6C23C396,0x67038BCA),
    MK_64(0x583A8BFC,0xCE34EB6C),
    MK_64(0x3FDBFB11,0xD4A46A3E),
    MK_64(0x3304ACFC,0xA8300998),
    MK_64(0xB2F6675F,0xA17F0FD2),
    MK_64(0x9D259973,0x0EF7AB6B),
    MK_64(0x0914A20D,0x3DFEA9E4),
    MK_64(0xCC1A9CAF,0xA494DBD3),
    MK_64(0x9828030D,0xA0A6388C),
    MK_64(0x0D339D5D,0xAADEE3DC),
    MK_64(0xFC46DE35,0xC4E2A086),
    MK_64(0x53D6E4F5,0x2E19A6D1),
    MK_64(0x5663952F,0x715D1DDD)
    };

#endif

#endif /* _SKEIN_IV_H_ */