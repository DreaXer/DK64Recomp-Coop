#include "common_structs.h"
#include "patches_main.h"
#include "ui.h"
#include "options.h"

#define gScissorUpLX D_global_asm_80744498
#define gScissorUpLY D_global_asm_8074449C
#define gScissorLowerRightX D_global_asm_807444A0
#define gScissorLowerRightY D_global_asm_807444A4

#define gScissor2LowerRightX D_global_asm_80744490
#define gScissor2LowerRightY D_global_asm_80744494

RECOMP_DECLARE_EVENT(recomp_adjust_dl_allocation(s32 *allocation));

RECOMP_PATCH void func_dk64_boot_8000102C(s32 offset, s32 size, void* dramAddr) {
    while (size & 0xf)
    {
        size++;
    }

    //@recomp: load overlays
    recomp_load_overlays(offset, dramAddr, size);

    osWritebackDCache(dramAddr, size);

    //@recomp: patch to call osPiRawStartDma variant
    //osPiRawStartDma(OS_READ, gOverlayTable[11].rom_code_start + offset, dramAddr, size);
    boot_osPiRawStartDma(OS_READ, gOverlayTable[11].rom_code_start + offset, dramAddr, size);

    do {} while (osPiGetStatus() & PI_STATUS_DMA_BUSY);
    osInvalDCache(dramAddr, size);
}

RECOMP_PATCH void func_dk64_boot_80000450(s32 devAddr, s32 arg1, void* dramAddr) {
    u32 size = arg1 - devAddr;

    //@recomp: load and map compressed address to uncompressed address
    load_dk64_overlay(devAddr, dramAddr, arg1 - devAddr);

    osInvalDCache(dramAddr, size);

    //@recomp: dma uncompressed data
    boot_osPiRawStartDma(OS_READ, devAddr, dramAddr, arg1 - devAddr);
    //osPiRawStartDma(OS_READ, devAddr, dramAddr, size);

    do {} while (osPiGetStatus() & PI_STATUS_DMA_BUSY);
}


RECOMP_PATCH void func_global_asm_80611730(void) {
    s32 corrupted = 0;
    s32 count;
    Unk807F0A58Entry* entry;

    func_global_asm_80611724(0x3791DFFF, 0x4BFFD668);
    //@recomp: patch to just remove this probable anti piracy check (it loads from uncached memory originally)
    //if (~0x3791DFFF != *(s32*)UNK_ADDR) {
    //    corrupted = 1;
    //}

    count = D_global_asm_807F5A58;
    if (count <= 0) {
        return;
    }

    entry = &D_global_asm_807F0A58[0];
    do {
        entry->unk4--;
        if (entry->unk4 == 0 && !corrupted) {
            func_global_asm_80611408((void*)entry->unk0);
            count = --D_global_asm_807F5A58;
            *entry = D_global_asm_807F0A58[count];
        }
        else {
            entry++;
        }
    } while (entry < &D_global_asm_807F0A58[count]);
}

extern int gameIsInDKTVMode(void);
#define DK_DKTV_LAG_START 215
#define DK_DKTV_LAG_END 325
#define DK_DKTV_LAG_START2 420
#define D_global_asm_807463AC (*(volatile s16*)0x807463AC)

void updateLag(s32 value) {
    s32 limit = 1;
    if ((is_cutscene_active != 3) && (is_cutscene_active != 4)) {
        limit = 2;
    }
    if (value < limit) {
        value = limit;
    }
    D_global_asm_80744478 = value;
}

RECOMP_PATCH void func_global_asm_80600674(void) {
    s32 max_boost = 1;
    s32 min_boost = 20;
    s32 newBoost;
    s32 pad;
    s32 cap;
    s32 idx;
    s32 updateLagBoost;
    u32 oldBoost;
    s32 i;
    Struct80767A40* osdata;
    u8 is_dktv = FALSE;

    //@recomp: patch to always greater than 1 (on console, default is 2. if zero, it will divide by zero and crash)
    AlterVolumes();

    //@recomp: patch to always greater than 1 (on console, default is 2. if zero, it will divide by zero and crash)

    if (D_global_asm_80744478 <= 1) {
        D_global_asm_80744478 = 2;
    }

    if ((gameIsInDKTVMode()) && (current_map == MAP_JAPES)) {
        // @recomp: DK's DKTV
        is_dktv = TRUE;
        D_global_asm_80744478 = 3;
    } else if (D_global_asm_8076AF14) {
        osdata = &D_global_asm_80767A40;
        newBoost = osdata->frame_count - D_global_asm_8076AF10;
        newBoost = MAX(1, newBoost);
        D_global_asm_8076AF00[D_global_asm_80745290++] = newBoost;
        updateLagBoost = FALSE;
        if (D_global_asm_80745290 == 8) {
            D_global_asm_80745290 = 0;
        }
        oldBoost = D_global_asm_80744478;
        if (oldBoost >= 4) {
            cap = 1;
        }
        else if (oldBoost < newBoost) {
            cap = 2;
        }
        else {
            cap = 4;
        }
        idx = D_global_asm_80745290;
        for (i = 0; i < cap; i++) {
            idx--;
            if (idx < 0) {
                idx = 7;
            }
            max_boost = MAX(max_boost, D_global_asm_8076AF00[idx]);
            min_boost = MIN(min_boost, D_global_asm_8076AF00[idx]);
        }
        if ((oldBoost < newBoost) && (oldBoost < min_boost)) {
            updateLagBoost = TRUE;
        }
        else if ((newBoost < oldBoost) && (max_boost < oldBoost)) {
            updateLagBoost = TRUE;
        }
        if (updateLagBoost) {
            //@recomp: dont update; stays at 2
            updateLag(newBoost);
        }
        if (object_timer > 10) {
            while (D_global_asm_8076AF10 + D_global_asm_80744478 > osdata->frame_count) {
                //@recomp: yield so this progresses correctly
                yield_self();
            }
        }
        D_global_asm_8076AF10 = osdata->frame_count;
        return;
    }
    osdata = &D_global_asm_80767A40;

    //@recomp: dont update; stays at 2
    if (!is_dktv) {
        updateLag(osdata->frame_count - D_global_asm_8076AF10);
    }

    D_global_asm_8076AF10 = osdata->frame_count;
    //recomp_printf("D_global_asm_80744478 is %d:\n", D_global_asm_80744478);
}


RECOMP_PATCH void func_dk64_boot_800009D0(void) {
    u32* tmp_a0;
    osInitialize();

    //@recomp: patch to cached read
    tmp_a0 = (void*)0x802FE1C0; 

    while (0xAD170014 != *tmp_a0);
    *tmp_a0 = 0xF0F0F0F0;
    func_dk64_boot_80000980();
}

Gfx *func_global_asm_805FE634(Gfx *dl, u8 arg1);
void func_global_asm_805FE71C(Gfx *dl, u8 arg1, s32 *arg2, u8 arg3);
extern void *_malloc(s32);
extern s32 D_global_asm_807FBB64;
extern s32 D_global_asm_8076A058;
extern Gfx *D_global_asm_8076A050[];
extern s32 D_global_asm_8076A088;

// @recomp: DL Allocation handler
RECOMP_PATCH void func_global_asm_805FE544(u8 arg0) {
    s32 temp;
    s32 stored_temp;
    // @recomp: Quadruple the DL Allocation
    if (D_global_asm_807FBB64 & 1) {
        temp = 24000;
        stored_temp = temp;
        recomp_adjust_dl_allocation(&temp);
        if (stored_temp > temp) {
            // Some mod reduced the allocation
            temp = stored_temp;
        }
        D_global_asm_8076A058 = temp;
    } else {
        temp = 12000;
        stored_temp = temp;
        recomp_adjust_dl_allocation(&temp);
        if (stored_temp > temp) {
            // Some mod reduced the allocation
            temp = stored_temp;
        }
        D_global_asm_8076A058 = arg0 * temp;
    }
    D_global_asm_8076A050[0] = _malloc(D_global_asm_8076A058 * sizeof(Gfx));
    D_global_asm_8076A050[1] = _malloc(D_global_asm_8076A058 * sizeof(Gfx));
    func_global_asm_805FE71C(func_global_asm_805FE634(D_global_asm_8076A050[0], 0), 0, &D_global_asm_8076A088, 1);
    func_global_asm_805FE71C(func_global_asm_805FE634(D_global_asm_8076A050[1], 1), 1, &D_global_asm_8076A088, 1);
}

//RECOMP_PATCH void func_global_asm_805FB5C4(OSMesgQueue* arg0, s32 arg1) {
//    OSTime target_time;
//    Struct80744464 sp34;
//    OSTime buffer_time;
//    u8 buffer[1];
//    void* sp20;
//    u8 buffer2[5];
//    static OSTime D_global_asm_807655E8;
//
//
//    sp34 = D_global_asm_80744464;
//    if (arg1 == 2) {
//        osViBlack(1);
//        func_global_asm_80601CF0(1);
//        D_global_asm_80744460 = 1;
//        while (TRUE) {}
//    }
//    osRecvMesg(arg0, &sp20, 1);
//    D_global_asm_80744460 = 1;
//    func_global_asm_80601CF0(1);
//    osStopThread(&D_global_asm_80761430); //this was previously patched to a osDestoryThread; is that neccessary?
//    osSetThreadPri(NULL, 0xB);
//    D_global_asm_807655E8 = osGetTime();
//    while (osGetTime() < D_global_asm_807655E8 + BUFFER_TIME);
//    osViBlack(1);
//
//    //@recomp:patch don't call these because?
//    __osSpSetStatus(0xAAAA82);
//    osDpSetStatus(0x1D6);
//
//    func_global_asm_8060E930();
//    while (TRUE) {}
//}

#define SCREEN_HEIGHT 240 // Normally 240
#define SCREEN_WIDTH 427 // Normally 320
#define OVERSCAN_SIZE 0  // Normally 10

// ---- Story co-op (experimental) ----
// 0 = native split-screen (AB8=0), 1 = shared single viewport (AB8=1)
#define COOP_SHARED_SCREEN 0
extern u8 D_global_asm_80750AB8;
extern u8 D_global_asm_80750AC0;
extern u8 D_global_asm_807ECDF8; // controllers active in adventure mode (bitmask)
extern u8 D_global_asm_807ECD09; // controllers connected (bitmask)
extern void func_global_asm_8060AA58(u8);

// Shops and Cranky's lab (single-player maps entered from DK Isles): the Kong who goes in plays alone (persona, colour, pad, text), the other one stays out
static int coop_is_shop(int m) { return m == MAP_FUNKYS_STORE || m == MAP_CANDYS_MUSIC_SHOP || m == MAP_SNIDES_HQ || m == MAP_CRANKYS_LAB; }
// Crown battle arenas (all ten; only Japes tested): not a boss, each player keeps his own Kong
static int coop_is_arena(int m) {
    return m == MAP_BATTLE_ARENA_BEAVER_BRAWL || m == MAP_BATTLE_ARENA_KRITTER_KARNAGE || m == MAP_BATTLE_ARENA_ARENA_AMBUSH || m == MAP_BATTLE_ARENA_MORE_KRITTER_KARNAGE
        || m == MAP_BATTLE_ARENA_FOREST_FRACAS || m == MAP_BATTLE_ARENA_BISH_BASH_BRAWL || m == MAP_BATTLE_ARENA_KAMIKAZE_KREMLINGS || m == MAP_BATTLE_ARENA_PLINTH_PANIC
        || m == MAP_BATTLE_ARENA_PINNACLE_PALAVER || m == MAP_BATTLE_ARENA_SHOCKWAVE_SHOWDOWN;
}
static int coop_map_allowed(void) {
    return current_map == MAP_TRAINING_GROUNDS || current_map == MAP_JAPES || current_map == MAP_JAPES_LOBBY || current_map == MAP_TROFF_N_SCOFF || current_map == MAP_DK_ISLES_OVERWORLD || current_map == MAP_AZTEC || current_map == MAP_JAPES_ARMY_DILLO || current_map == MAP_KLUMSY || coop_is_arena(current_map); // co-op maps
}

// Playable Kongs: 1 if the Kong (0 DK, 1 Diddy, 2 Lanky, 3 Tiny, 4 Chunky) is unlocked in the current save
// (permanent flags of the "Kong unlocked" table, the ones shown as "?" on the Kong select screens when still locked).
extern s32 D_global_asm_80744720[];
extern u8 isFlagSet(s16 flagIndex, u8 flagType);
int coop_kong_unlocked(int kong) {
    if (kong < 0 || kong > 4) return 0;
    return isFlagSet((s16)D_global_asm_80744720[kong], FLAG_TYPE_PERMANENT) ? 1 : 0;
}
// bitmask of the unlocked Kongs (bit 0 DK ... bit 4 Chunky)
int coop_kongs_unlocked_mask(void) {
    int k, m = 0;
    for (k = 0; k < 5; k++) if (coop_kong_unlocked(k)) m |= 1 << k;
    return m;
}

RECOMP_PATCH void func_global_asm_805FB944(u8 arg0) {
    u8 var_a1 = 1;
    s32 var_a2;

    var_a2 = 0;
    func_global_asm_806003EC(D_global_asm_8076A0AA);
    //@recomp Patch to always be 240p by setting D_global_asm_8074450C to 1
    if (current_map == MAP_NINTENDO_LOGO) {
        D_global_asm_8074450C = 1;
    }
    else {
        D_global_asm_8074450C = 1;
    }
    switch (is_cutscene_active) {
    case 3:
        var_a1 = 9;
    case 4:
        if (var_a1 == 1) {
            var_a1 = 0xA;
        }
        gScissorUpLX = 0;
        gScissorUpLY = 0;
        gScissorLowerRightX = (D_global_asm_8074450C * 320);
        gScissorLowerRightY = (D_global_asm_8074450C * 240);
        break;
    default:
        var_a2 = func_global_asm_8060042C(current_map);
        var_a1 = 1;
        if (D_global_asm_807FBB64 & 1) {
            var_a1 = 7;
        }
        else if (D_global_asm_807FBB64 & 0x1000) {
            var_a1 = 6;
        }
        else if (D_global_asm_807FBB64 & 0x104000) {
            var_a1 = 8;
        }
        else if (D_global_asm_807FBB64 & 0x80000) {
            var_a1 = 4;
        }
        else if (D_global_asm_807FBB64 & 0x2000) {
            var_a1 = 5;
        }
        else if (D_global_asm_807FBB64 & 0x04000000) {
            var_a1 = 3;
        }
        else if (D_global_asm_807FBB64 & 0x40000000) {
            var_a1 = 2;
        }
        gScissorUpLX = D_global_asm_8074450C * OVERSCAN_SIZE;
        gScissorUpLY = D_global_asm_8074450C * OVERSCAN_SIZE;
        gScissorLowerRightX = (D_global_asm_8074450C * (320 - OVERSCAN_SIZE));
        gScissorLowerRightY = (D_global_asm_8074450C * (240 - OVERSCAN_SIZE));
        break;
    }
    //@recomp Story co-op: set AFTER the overlay choice above, so the story overlay stays loaded.
    //The flag only needs to survive until the reset check in func_global_asm_805FE7FC.
    if (recomp_get_coop_enabled() && coop_map_allowed() && is_cutscene_active == 0) {
        D_global_asm_807FBB64 |= 0x04000000;
        D_global_asm_80750AC0 = 2;
        D_global_asm_80750AB8 = COOP_SHARED_SCREEN;
        // Starting a story game restricts input to port 1 (mask 1): other ports get no
        // "newly pressed" buttons. Enable ports 1+2 so P2 gets full input.
        if ((D_global_asm_807ECDF8 & 3) != (D_global_asm_807ECD09 & 3)) {
            func_global_asm_8060AA58(3);
        }
    }
    func_global_asm_80610350(arg0, var_a1, var_a2);
    if (D_global_asm_807445A4 == 0) {
        osViSetMode(&osViModeTable[D_global_asm_80744588[osTvType + osTvType + D_global_asm_8074450C - 1]]);
        if (D_global_asm_807445A0 == 0) {
            osViBlack(1U);
        }
        D_global_asm_80744510 = 0;
        D_global_asm_807445A0 = 0;
    }
    else {
        D_global_asm_80744510 = 1;
        D_global_asm_807445A0 = 1;
        D_global_asm_807445A4 = 0;
        func_global_asm_805FB7E4();
    }
    osViSetSpecialFeatures(VI_CTRL_TYPE_16 | VI_CTRL_SERRATE_ON);
    gScissor2LowerRightX = D_global_asm_8074450C * 320; //width
    gScissor2LowerRightY = D_global_asm_8074450C * 240; //height   
}

extern OSMesg decompression_mq[];

RECOMP_PATCH void func_global_asm_805FBC5C(void) {
    UnkMQStruct* mq;
    D_global_asm_8076A084 = gOverlayTable[12].rom_data_end - gOverlayTable[12].rom_code_start;
    osCreateMesgQueue(&D_global_asm_807655F0.mq, &D_global_asm_807655F0.msgs[0], 0x32);
    osCreateMesgQueue(&D_global_asm_807656D0.mq, &decompression_mq[0], DECOMPRESSION_BUFFER_SIZE);
    func_global_asm_8060EC80(
        &D_global_asm_80767A40.queue,
        &D_global_asm_80767A40,
        0x19,
        D_global_asm_80744588[osTvType + osTvType], 1);
    osCreateMesgQueue(&D_global_asm_807659E8.mq, &D_global_asm_807659E8.msgs[0], 0x10);
    func_global_asm_8060ED6C(
        (void*) &D_global_asm_80767A40,
        (void*)&D_global_asm_80767CD8,
        (s32) &D_global_asm_807659E8, 1, 1);
    current_map = next_map;
    func_global_asm_805FB944(0);
    D_global_asm_8076A07C = 5;
    func_global_asm_8060FFF0();
    func_global_asm_8060A900();
    func_global_asm_80600D50();
    setIntroStoryPlaying(0);
    func_global_asm_8073239C();
    mq = (void*)&D_global_asm_8076A110;
    osCreateMesgQueue((void*)mq, &D_global_asm_8076A108, 2);
    //@recomp: patch this timer to be significantly faster; we dont need to wait long
    osSetTimer(&D_global_asm_8076A130, OS_USEC_TO_CYCLES(100000), 0, (void*)mq, mq->msgs[0]); //wait 0.1 seconds
    //@recomp move playsound to a bit later
    //playSound(0x23C, 0x7FFF, 63.0f, 1.0f, 0, 0);
}

extern s32 D_global_asm_807F5E64;
extern f32 D_global_asm_807F5FA8;
extern f32 D_global_asm_807F5FAC;
extern f32 D_global_asm_807F5FB0;
extern f32 D_global_asm_807F5FB4;
extern u8 cc_player_index; // index into character_change_array, current_character_index[]
extern f32 D_global_asm_807F5FE0;
extern f32 D_global_asm_807F5FDC;
extern f32 D_global_asm_807F5E68;
extern f32 D_global_asm_807F5E20[][3];
extern s32 D_global_asm_807F5DE4; // TODO: Actually a pointer to a struct (map model?)
extern s32 D_global_asm_807F5E60;
extern u8 D_global_asm_807F5FEC;
extern s32 D_global_asm_807F5FF0;
extern u8 D_global_asm_80750AB4;
extern void* D_global_asm_807F5DE8;
extern void* D_global_asm_807F5DEC;

f32 func_global_asm_80612D10(f32 arg0);
void func_global_asm_8062DB70(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5);
void func_global_asm_8062A944(f32 arg0, f32 arg1, f32 arg2);
void func_global_asm_8062AC68(void* arg0);
void func_global_asm_8062AD28(f32 arg0, f32 arg1, f32 arg2, void* arg3, f32* arg4);
void func_global_asm_8062D620(s32, s32, s32, f32, f32, f32, s32, s32, s32); //first and second arg here was s32, now void*
Gfx* func_global_asm_80722294(Gfx*, Actor*, s16);
void func_global_asm_8062C99C(CharacterChange250*, s32, s32, s32, s32);
Gfx* func_global_asm_8065919C(Gfx* dl);
Gfx* func_global_asm_8070835C(Gfx*, u8);
extern Mtx D_2000180;
Gfx* func_global_asm_8062CA70(Gfx* dl, s32 arg1, s32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6);

extern s32 func_global_asm_80626F8C(f32 arg0, f32 arg1, f32 arg2, f32 *arg3, f32 *arg4, s32 arg5, f32 arg6, s32 arg7);
//@recomp: Seems to be used for the culling of many objects, including actors and props
RECOMP_PATCH s32 func_global_asm_80658E8C(f32 arg0, f32 arg1, f32 arg2, u8 arg3, u8 arg4) {
    f32 sp44;
    f32 sp40;
    s16 sp3E;
    s16 sp3C;
    f32 d;
    f32 temp_f2_2;
    s16 var_a0;
    s16 var_a1;
    s16 var_a2;
    s16 var_v1;

    func_global_asm_80626F8C(arg0, arg1, arg2, &sp44, &sp40, 0, 1.0f, cc_player_index);
    sp3E = sp44;
    sp3C = sp40;
    d = _sqrtf(
        SQ(arg0 - character_change_array[cc_player_index].unk21C) +
        SQ(arg1 - character_change_array[cc_player_index].unk220) +
        SQ(arg2 - character_change_array[cc_player_index].unk224)
    );
    if (d < 90.0f) {
        return 0;
    }
    if (d >= 180.0f) {
        var_v1 = 0x14;
        var_a0 = 0x14;
        var_a1 = 0;
        var_a2 = 0x32;
    } else {
        temp_f2_2 = (d - 60.0f) / 120.0f;
        if (temp_f2_2 > 0.0) {
            var_v1 = (-60.0f * temp_f2_2) + 80.0f;
            var_a0 = (-60.0f * temp_f2_2) + 80.0f;
            var_a1 = (-130.0f * temp_f2_2) + 130.0f;
            var_a2 = (-80.0f * temp_f2_2) + 130.0f;
        } else {
            var_v1 = 0x50;
            var_a0 = 0x50;
            var_a1 = 0x82;
            var_a2 = 0x82;
        }
    }
    // @recomp: Remove x bound checks
    if ((sp3C < ((D_global_asm_807F735A - var_a1) - arg4)) || ((((D_global_asm_807F735E + var_a2 + arg4) < sp3C)))) {
        return 1;
    } else {
        return 0;
    }
}



//@recomp: Sprite culling. Also used for the scissor of the sprite itself
RECOMP_PATCH void func_global_asm_80714A68(s16 arg0, s16 arg1, s16 arg2, s16 arg3) {
    D_global_asm_807FDB3C = OVERSCAN_SIZE;
    D_global_asm_807FDB3E = OVERSCAN_SIZE;
    D_global_asm_807FDB40 = (320 - OVERSCAN_SIZE);
    D_global_asm_807FDB42 = (240 - OVERSCAN_SIZE);
}

//@recomp: Used for a bunch of display list initialization
RECOMP_PATCH Gfx *func_global_asm_807132DC(Gfx *dl) {
    dl = func_global_asm_805FD030(dl);
    gSPDisplayList(dl++, &D_1000118);
    gSPMatrix(dl++, &D_20000C0, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPPipeSync(dl++);
    gDPSetCombineMode(dl++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, OVERSCAN_SIZE, OVERSCAN_SIZE, 320 - OVERSCAN_SIZE, 240 - OVERSCAN_SIZE);
    return dl;
}

//@recomp: Sprite reset function. Adjust scissor variables to not be tied to the original screen dimensions
RECOMP_PATCH void func_global_asm_80714A9C(void) {
    D_global_asm_807FDB0F = 0;
    D_global_asm_807FDB10 = 1;
    D_global_asm_807FDB14 = 0;
    D_global_asm_807FDB18 = 0;
    D_global_asm_807FDB1C = 1;
    D_global_asm_807FDB1A = 0;
    D_global_asm_807FDB1D = 0;
    D_global_asm_807FDB28 = 0;
    D_global_asm_807FDB2C = 0;
    D_global_asm_807FDB30 = 0;
    D_global_asm_807FDB36 = 0;
    D_global_asm_807FDB38 = -1;
    D_global_asm_807FDB3C = D_global_asm_8074450C * OVERSCAN_SIZE;
    D_global_asm_807FDB3E = D_global_asm_8074450C * OVERSCAN_SIZE;
    D_global_asm_807FDB40 = D_global_asm_8074450C * ((320 - OVERSCAN_SIZE));
    D_global_asm_807FDB42 = D_global_asm_8074450C * ((240 - OVERSCAN_SIZE));
    D_global_asm_807FDB3A = 0x258;
}

Gfx* func_global_asm_8062CEA8(Gfx*, void*, u8);      /* extern */
Gfx* func_global_asm_8063A968(Gfx*, s32);           /* extern */
void* func_global_asm_80656B98(Gfx*, s32, s32);       /* extern */
Gfx* func_global_asm_8065D994(Gfx*, s16);           /* extern */
void* func_global_asm_8065FD88(Gfx*, s32, s32);       /* extern */

typedef struct Struct807F6C0C {
    s16 unk0;
    s16 unk2;
    s16 unk4;
    s16 unk6;
    void *unk8;
    void *unkC;
} Struct807F6C0C;

typedef struct {
    u8 loaded;
    u8 unk1;
    u8 unk2;
    u8 unk3;
    u8 unk4;
    u8 visible;
    u8 unk6;
    u8 unk7;
    s32 unk8;
    s32 unkC;
    s32 unk10;
    void *unk14;
    void *unk18;
    void *unk1C;
    void *unk20;
    s32 unk24;
    u8 pad28[0x2C - 0x28];
    s32 unk2C;
    s32 unk30;
    s32 unk34;
    s32 unk38;
    u8 pad3C[0x4C - 0x3C];
    void *unk4C;
    s32 unk50;
    u8 pad54[0x58 - 0x54];
    void *unk58;
    u8 pad5C[0x60 - 0x5C];
    s32 unk60[1];
    s32 unk64;
    s32 deload1;
    s32 deload2;
    s32 deload3;
    s32 deload4;
    void *unk78;
    void *unk7C;
    s16 unk80;
    s16 unk82;
    s16 unk84;
    s16 unk86;
    u8 pad2[0x1C8 - 0x88];
} Chunk;

typedef struct Struct80630B70 Struct80630B70;
struct Struct80630B70 {
    Actor *unk0;
    s32 unk4;
    s32 unk8;
    f32 unkC;
    u8 pad10[0x14-0x10];
    Struct80630B70 *unk14;
    u8 pad18[0x24 - 0x18];
    u8 unk24;
};

extern void* D_global_asm_807F5DE8;
extern void* D_global_asm_807F5DEC;
extern u8 D_global_asm_807F5FEC;
extern Struct80630B70* D_global_asm_807F5FFC;
extern u8 D_global_asm_807F6009;
extern s32 D_global_asm_807F600C;
extern s16 D_global_asm_807F6BF0[];
extern u8 D_global_asm_807F6C08;
extern Struct807F6C0C* D_global_asm_807F6C0C;
extern s16 D_global_asm_807F6C58[];
extern s32 D_global_asm_807F6C80;
extern void* D_global_asm_807F7074;
extern Chunk *chunk_array_pointer;
extern void func_global_asm_8062EE48(u8 arg0);
extern Gfx *func_critter_80027034(Gfx *dl);
extern Gfx *func_global_asm_806634A4(Gfx *dl);
extern Gfx *func_global_asm_80630B70(Gfx*, void *, f32, f32, f32, s32, s16, u8);
extern Gfx *func_global_asm_806592B4(Gfx *dl);
extern Gfx *func_global_asm_8062EDA8(Gfx *dl, u8 arg1);

typedef struct Struct80655DD0_arg1 {
    s32 unk0;
    s32 unk4;
    void *unk8;
} Struct80655DD0_arg1;

Gfx *determineChunkFix(Gfx *dl, Chunk *ch) {
    if (cc_number_of_players == 1) {
        // Needed for screen squishes (lol)
        gDPSetScissor(dl++, G_SC_NON_INTERLACE,
            0,
            character_change_array[cc_player_index].unk270[1],
            D_global_asm_80744490,
            character_change_array[cc_player_index].unk270[3]);
    } else {
        gDPSetScissor(dl++, G_SC_NON_INTERLACE,
            character_change_array[cc_player_index].unk270[0],
            character_change_array[cc_player_index].unk270[1],
            character_change_array[cc_player_index].unk270[2],
            character_change_array[cc_player_index].unk270[3]);
    }
    func_global_asm_80658E58(
        ch->deload1,
        ch->deload2,
        ch->deload3,
        ch->deload4);
    return dl;
}

// @recomp: Chunk bounds fix
RECOMP_PATCH Gfx *func_global_asm_80655DD0(Gfx * dl, Struct80655DD0_arg1 * arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    s32 i;
    s32 var_s6;
    void *var_a1;
    s32 sp60;

    var_s6 = 0;
    sp60 = D_global_asm_807F5FEC && character_change_array[cc_player_index].fov_y == 45.0;
    gDPPipeSync(dl++);
    if (arg5 & 0x10) {
        gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(dl++, 0x06, osVirtualToPhysical(D_global_asm_807F5DE8));
        gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
        dl = func_global_asm_8062CEA8(dl, arg1, 1);
        if (arg1->unk8 != (void*)-1) {
            gDPPipeSync(dl++);
            gSPDisplayList(dl++, osVirtualToPhysical(arg1->unk8));
        }
        dl = func_global_asm_8065FD88(dl, 0, 0);
        for (i = 0; i < D_global_asm_807F6C08; i++) {
            if (D_global_asm_807F6C0C[D_global_asm_807F6BF0[i]].unk8 != (void*)-1) {
                gDPPipeSync(dl++);
                gSPDisplayList(dl++, osVirtualToPhysical(D_global_asm_807F6C0C[D_global_asm_807F6BF0[i]].unk8));
            }
            if (D_global_asm_807F6C0C[D_global_asm_807F6BF0[i]].unkC != (void*)-1) {
                gDPPipeSync(dl++);
                gSPDisplayList(dl++, osVirtualToPhysical(D_global_asm_807F6C0C[D_global_asm_807F6BF0[i]].unkC));
            }
        }
        gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    }
    for (i = D_global_asm_807F6C80 - 1; i >= 0; i--) {
        var_s6 = D_global_asm_807F6C58[i];
        if (chunk_array_pointer[var_s6].loaded == 1) {
            if (chunk_array_pointer[var_s6].unk2 != 0) {
                func_global_asm_8062EE48(var_s6);
                chunk_array_pointer[var_s6].unk2 = 0;
            }
            gDPPipeSync(dl++);
            dl = determineChunkFix(dl, &chunk_array_pointer[var_s6]);
            dl = func_global_asm_806592B4(dl);
            if (arg5 & 0x10) {
                dl = func_global_asm_8062EDA8(dl, var_s6);
                if (chunk_array_pointer[var_s6].unk2C != -1) {
                    gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPSegment(dl++, 0x06, osVirtualToPhysical(chunk_array_pointer[var_s6].unk58));
                    gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
                    dl = func_global_asm_80656B98(dl, var_s6, 0);
                    gDPPipeSync(dl++);
                    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                }
                if (chunk_array_pointer[var_s6].unk34 != -1) {
                    gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPSegment(dl++, 0x06, osVirtualToPhysical(chunk_array_pointer[var_s6].unk58));
                    gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
                    dl = func_global_asm_80656B98(dl, var_s6, 2);
                    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                }
            }
            gDPPipeSync(dl++);
            if (!(D_global_asm_807FBB64 & 0x01000000)) {
                dl = func_global_asm_8065D994(dl, var_s6);
            }
            dl = func_global_asm_8063A968(dl, chunk_array_pointer[var_s6].unk24);
            D_global_asm_807F6009 = 0xFF;
            var_a1 = ((u8)sp60) ? chunk_array_pointer[var_s6].unk18 : chunk_array_pointer[var_s6].unk14;
            dl = func_global_asm_80630B70(dl, var_a1, arg2, arg3, arg4, arg5, var_s6, sp60);
            if (arg5 & 0x10) {
                gSPLoadGeometryMode(dl++, 0);
                gSPSetGeometryMode(dl++, G_SHADE | G_SHADING_SMOOTH);
                dl = func_global_asm_806592B4(dl);
                dl = func_global_asm_8062EDA8(dl, var_s6);
                if (chunk_array_pointer[var_s6].unk30 != -1) {
                    gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPSegment(dl++, 0x06, osVirtualToPhysical(chunk_array_pointer[var_s6].unk58));
                    gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
                    dl = func_global_asm_80656B98(dl, var_s6, 1);
                    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gDPPipeSync(dl++);
                }
                if (chunk_array_pointer[var_s6].unk38 != -1) {
                    gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    gSPSegment(dl++, 0x06, osVirtualToPhysical(chunk_array_pointer[var_s6].unk58));
                    gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
                    dl = func_global_asm_80656B98(dl, var_s6, 3);
                    gDPPipeSync(dl++);
                    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                }
            }
        }
    }
    if (arg5 & 0x10) {
        gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(dl++, 0x06, osVirtualToPhysical(D_global_asm_807F5DE8));
        gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
        dl = func_global_asm_8062CEA8(dl, arg1, 2);
        if (D_global_asm_807FBB64 & 0x1000) {
            gDPPipeSync(dl++);
            dl = func_critter_80027034(dl);
            gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        }
        dl = func_global_asm_8065FD88(dl, var_s6, 1);
        gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(dl++);
        dl = func_global_asm_806634A4(dl);
    }
    for (i = D_global_asm_807F6C80 - 1; i >= 0; i--) {
        var_s6 = D_global_asm_807F6C58[i];
        if (chunk_array_pointer[var_s6].loaded == 1) {
            gDPPipeSync(dl++);
            dl = determineChunkFix(dl, &chunk_array_pointer[var_s6]);
            D_global_asm_807F6009 = 0xFF;
            var_a1 = ((u8)sp60) ? chunk_array_pointer[var_s6].unk20 : chunk_array_pointer[var_s6].unk1C;
            dl = func_global_asm_80630B70(dl, var_a1, arg2, arg3, arg4, arg5, var_s6, sp60);
            if (D_global_asm_807FBB64 & 0x01000000) {
                dl = func_global_asm_8065D994(dl, var_s6);
            }
        }
    }
    if (arg5 & 0x10) {
        gSPMatrix(dl++, osVirtualToPhysical(D_global_asm_807F7074), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPSegment(dl++, 0x06, osVirtualToPhysical(D_global_asm_807F5DE8));
        gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
        dl = func_global_asm_8062CEA8(dl, arg1, 3);
        gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPPipeSync(dl++);
    }
    gDPPipeSync(dl++);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE,
        character_change_array[cc_player_index].unk270[0],
        character_change_array[cc_player_index].unk270[1],
        character_change_array[cc_player_index].unk270[2],
        character_change_array[cc_player_index].unk270[3]);
    func_global_asm_80658E58(
        character_change_array[cc_player_index].unk270[0],
        character_change_array[cc_player_index].unk270[1],
        character_change_array[cc_player_index].unk270[2],
        character_change_array[cc_player_index].unk270[3]);
    D_global_asm_807F600C = 0;
    dl = func_global_asm_80630B70(dl, D_global_asm_807F5FFC, arg2, arg3, arg4, arg5, -1, 0U);
    dl = func_global_asm_8065D994(dl, -1);
    return dl;
}

extern int coop_wx_two(void);
extern int coop_wx_range(int owner, int *top, int *bottom);
extern f32 coop_ship_f(int p);
static int coop_active_here(void);

//@recomp: Water Screen Overlay
RECOMP_PATCH Gfx *func_global_asm_80701CA0(Gfx *dl) {
    CameraPaad* camera_paad;
    PlayerAdditionalActorData* player_aad;
    f32 var_f2;
    u8 spC3;
    u8 var_fp;
    s8 spBA;
    u8 i;
    Mtx* sp6C;
    Mtx* sp68;

    spC3 = FALSE;
    var_fp = 0x64;
    spBA = FALSE;
    if (current_map == MAP_DK_ISLES_OVERWORLD && coop_wx_two()) {
        // @recomp co-op: darkening near K. Rool's ship, per Kong. The original dims the ambient light of the whole scene (one value
        // for both views: see func_global_asm_80707980), so here it is a translucent black veil over the view of the Kong near the ship.
        int p;
        int veil = 0;
        for (p = 0; p < 2; p++) {
            f32 f = coop_ship_f(p);
            int top, bot;
            if (f > 0.0f && coop_wx_range(p, &top, &bot)) {
                if (!veil) {
                    gDPPipeSync(dl++);
                    dl = func_global_asm_805FCFD8(dl);
                    gDPSetScissor(dl++, G_SC_NON_INTERLACE, gScissorUpLX, gScissorUpLY, gScissorLowerRightX, gScissorLowerRightY);
                    gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                    gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
                    gDPSetCycleType(dl++, G_CYC_1CYCLE);
                    gSPMatrix(dl++, &D_2000080, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
                    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                    veil = 1;
                }
                gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x00, (u8)(f * 0.6f * 255.0f));
                gDPFillRectangle(dl++, character_change_array[p].unk270[0], top, character_change_array[p].unk270[2], bot);
                gDPPipeSync(dl++);
            }
        }
        if (veil) {
            gSPMatrix(dl++, &D_2000000, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
            gSPMatrix(dl++, &D_2000200, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
            gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        }
    }
    switch (current_map) {
        case MAP_GALLEON_MERMAID:
            spBA = TRUE;
            goto block_12;
        case MAP_GALLEON_SUBMARINE:
        case MAP_GALLEON_SHIPWRECK_DIDDY_LANKY_CHUNKY:
        case MAP_GALLEON_SHIPWRECK_DK_TINY:
        case MAP_GALLEON_SHIPWRECK_LANKY_TINY:
            return dl;
        case MAP_GALLEON:
            if (((character_change_array->chunk == 9) || (character_change_array->chunk == 0xA)) && (isFlagSet(0x9C, 0U))) {
                return dl;
            }
        default:
    block_12:
            // @recomp co-op: the game has a single player slot, so also tint J2's view
            for (i = 0; i < (coop_active_here() ? 2 : cc_number_of_players); i++) {
                if (character_change_array[i].does_player_exist && character_change_array[i].playerPointer != NULL) {
                    player_aad = character_change_array[i].playerPointer->PaaD;
                    camera_paad = player_aad->unk104->CaaD;
                    if (spBA || (
                        character_change_array[i].unk2E8 && 
                        (character_change_array[i].unk220 < (character_change_array[i].unk24C + 3.0f))
                    )) {
                        spC3 = TRUE;
                        gDPPipeSync(dl++);
                        dl = func_global_asm_805FCFD8(dl);
                        gDPSetScissor(dl++, G_SC_NON_INTERLACE,
                            gScissorUpLX,
                            gScissorUpLY,
                            gScissorLowerRightX,
                            gScissorLowerRightY);
                        gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
                        gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
                        var_f2 = character_change_array[i].unk24C - character_change_array[i].unk220;
                        switch (current_map) {
                            case MAP_GALLEON:
                                var_f2 *= 0.07;
                                break;
                            case MAP_GALLEON_TREASURE_CHEST:
                                var_fp = 50;
                                var_f2 *= 0.07;
                                break;
                            case MAP_GALLEON_PUFFTOSS:
                                var_fp = 50;
                            default:
                                var_f2 *= 0.4;
                                break;
                        }
                        if (var_f2 > 80.0f) {
                            var_f2 = 80.0f;
                        }
                        if (D_global_asm_807FD890) {
                            player_aad->unk1E8 = var_f2;
                        } else {
                            player_aad->unk1E8 += ((var_f2 - player_aad->unk1E8) * 0.05);
                            var_f2 = (u8) player_aad->unk1E8;
                        }
                        gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x3C, (u8)(var_fp + var_f2));
                        gDPSetCycleType(dl++, G_CYC_1CYCLE);\
                        gSPMatrix(dl++, &D_2000080, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
                        gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                        //@recomp: Used to be a verts-based draw, now is just fillrect
                        gDPFillRectangle(dl++,
                            character_change_array[i].unk270[0],
                            character_change_array[i].unk270[1],
                            character_change_array[i].unk270[2],
                            character_change_array[i].unk270[3]
                        );
                        gDPPipeSync(dl++);
                    }
                    character_change_array[i].unk2E8 = camera_paad->unkFA;
                    character_change_array[i].unk24C = camera_paad->unk90;
                }
            }
            if (spC3) {
                gSPMatrix(dl++, &D_2000000, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
                gSPMatrix(dl++, &D_2000200, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
                gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                D_global_asm_807FD890 = 0;
            } else {
                D_global_asm_807FD890 = 1;
            }
            break;
    }
    return dl;
}

//@recomp: Sandstorm screen overlay
RECOMP_PATCH Gfx* func_global_asm_8068D264(Gfx* dl, f32* cooldown_timer) {
    void* temp_v0;
    f32 cooldown;
    f32 half_lag;

    cooldown = *cooldown_timer;
    half_lag = D_global_asm_80744478 * 0.5;
    temp_v0 = getPointerTableFile(TABLE_25_TEXTURES_GEOMETRY, 0x173C, 1U, 0U);
    func_global_asm_8066B434(temp_v0, 0x1B2, 0x46);
    gDPPipeSync(dl++);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, \
        character_change_array->unk270[0], \
        character_change_array->unk270[1], \
        character_change_array->unk270[2], \
        character_change_array->unk270[3]);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetTextureLOD(dl++, G_TL_LOD);
    gSPLoadGeometryMode(dl++, 0);
    gSPSetGeometryMode(dl++, G_SHADE | G_SHADING_SMOOTH);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(dl++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    if (current_map == MAP_AZTEC) {
        gDPSetPrimColor(dl++, 0, 0, 0x8A, 0x52, 0x16, 200.0f * cooldown);
    } else {
        gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 35.0f * cooldown);
    }
    gDPLoadTextureBlock(dl++, OS_PHYSICAL_TO_K0(temp_v0), G_IM_FMT_IA, G_IM_SIZ_8b, 64, 64, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, 6, 6, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(
        dl++,
        character_change_array->unk270[0] * 4,
        character_change_array->unk270[1] * 4,
        character_change_array->unk270[2] * 4,
        character_change_array->unk270[3] * 4,
        G_TX_RENDERTILE,
        (s32)(D_global_asm_8075022C * 8.0f),
        (s32)(D_global_asm_80750228 * 8.0f),
        -1024,
        1024
    );
    gDPPipeSync(dl++);

    if (current_map == MAP_AZTEC) {
        D_global_asm_80750228 -= (2.0 * half_lag);
        D_global_asm_8075022C -= (14.0 * half_lag);
    } else {
        D_global_asm_80750228 -= (f64)half_lag;
        D_global_asm_8075022C -= (0.5 * half_lag);
    }
    if (D_global_asm_80750228 < 0.0) {
        D_global_asm_80750228 += 255.0f;
    }
    if (D_global_asm_8075022C < 0.0) {
        D_global_asm_8075022C += 255.0f;
    }
    return dl;
}

//@recomp: Patch "wrong cutscene" fade transition to match func_global_asm_80703374 (roughly)
RECOMP_PATCH Gfx *func_global_asm_80703AB0(Gfx *dl, u8 arg1) {
    if (arg1 != 0) {
        gSPClearGeometryMode(dl++, G_ZBUFFER | G_SHADE | G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_LOD | G_SHADING_SMOOTH | G_CLIPPING | 0x0040F9FA);
        gSPSetGeometryMode(dl++, G_SHADE | G_SHADING_SMOOTH);
        gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
        gDPPipeSync(dl++);
        gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x00, arg1);
        gDPSetCycleType(dl++, G_CYC_1CYCLE);
        gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        gDPFillRectangle(dl++, gScissorUpLX, gScissorUpLY, gScissorLowerRightX, gScissorLowerRightY);
    }
    gDPPipeSync(dl++);
    return dl;
}

// @recomp: Patch the static effect in the Rap to fill the full bounds
RECOMP_PATCH Gfx *func_global_asm_807035C4(Gfx *dl, Actor *arg1) {
    s16 var_s1;
    s16 temp_s2;
    s16 temp_t2;

    gDPPipeSync(dl++);
    gDPSetRenderMode(dl++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    gDPSetPrimColor(dl++, 0, 0, 0xC8, 0xC8, 0xC8, 0x80);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    gDPSetCombineLERP(dl++, NOISE, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, NOISE, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, character_change_array->unk270[0], character_change_array->unk270[1], character_change_array->unk270[2], character_change_array->unk270[3]);
    gDPFillRectangle(dl++, character_change_array->unk270[0], character_change_array->unk270[1], character_change_array->unk270[2], character_change_array->unk270[3]);
    gDPPipeSync(dl++);
    gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x00, 0xFF);
    gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    for (var_s1 = 0; var_s1 < 10; var_s1++) {
        temp_s2 = (RANDNUM() >> 0xF) % (character_change_array->unk270[3] - character_change_array->unk270[1]) + character_change_array->unk270[1];
        temp_t2 = ((RANDNUM() >> 0xF) % 10) + 2;
        gDPFillRectangle(dl++, character_change_array->unk270[0], temp_s2, character_change_array->unk270[2], temp_s2 + temp_t2);
    }
    gDPPipeSync(dl++);
    return dl;
}

RECOMP_PATCH s32 func_global_asm_806522CC(s16 arg0, s16 arg1, s16 arg2) {
    return 1;
}

#define macro_8062DBDC_IF(a0, a1, a2) ((a0 * D_global_asm_807F5E50[0]) + (a1 * D_global_asm_807F5E50[1]) + (a2 * D_global_asm_807F5E50[2])) + D_global_asm_807F5E50[3]

//@recomp: Culling patch for maps
RECOMP_PATCH s32 func_global_asm_8062DBDC(s16 arg0, s16 arg1, s16 arg2, s16 arg3, s16 arg4, s16 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9, Struct8062DBDC *arg10) {
    s32 var_a0;
    s32 var_a1;
    s32 var_a2;
    s32 i;
    s32 pad[3];
 
    if (macro_8062DBDC_IF(arg0, arg1, arg2) < 0.0) {
        if (macro_8062DBDC_IF(arg0, arg1, arg5) < 0.0) {
            if (macro_8062DBDC_IF(arg0, arg4, arg2) < 0.0) {
                if (macro_8062DBDC_IF(arg0, arg4, arg5) < 0.0) {
                    if (macro_8062DBDC_IF(arg3, arg1, arg2) < 0.0) {
                        if (macro_8062DBDC_IF(arg3, arg1, arg5) < 0.0) {
                            if (macro_8062DBDC_IF(arg3, arg4, arg2) < 0.0) {
                                if (macro_8062DBDC_IF(arg3, arg4, arg5) < 0.0) {
                                    return TRUE;  //@recomp: Was false
                                 }
                            }
                        }
                    }
                }
            }
        }
    }
    var_a0 = FALSE;
    var_a1 = FALSE;
    var_a2 = FALSE;
    if ((arg0 <= arg6) && (arg6 <= arg3) && (arg1 <= arg7) && (arg7 <= arg4) && (arg2 <= arg8) && (arg8 <= arg5)) {
        return TRUE;
    }
    for (i = 0; (i < 5) && (!var_a2); i++) {
        if ((arg10[i].unk0[7] + (((arg10[i].unk0[4] * arg0) + (arg10[i].unk0[5] * arg1)) + (arg10[i].unk0[6] * arg2))) < 0) {
            var_a0 = TRUE;
        }
        if ((arg10[i].unk0[7] + (((arg10[i].unk0[4] * arg0) + (arg10[i].unk0[5] * arg4)) + (arg10[i].unk0[6] * arg2))) < 0) {
            var_a1 = TRUE;
        }
        if ((var_a0) && (var_a1)) {
            var_a2 = TRUE;
        }
    }
    if ((!var_a2) && (func_global_asm_8062E040(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8) < arg9)) {
        return TRUE;
    }
    for (i = 0; i < 5; i++) {
        if (func_global_asm_8062E1F8(i, arg0, arg1, arg2, arg3, arg4, arg5, arg10)) {
            return TRUE;
        }
    }
    return TRUE;  // @recomp: Was false
}

//@recomp: Audio buffer get
RECOMP_PATCH s32 func_global_asm_80601EE4(Struct8076D708 *arg0, Struct8076D708 *lastInfo) {
    s16 *audioPtr;
    s32 var_v1; // samples left?
    s32 sp34;
    s32 *new_var;
    Acmd *temp_v1; // cmdp
    s32 temp;

    audioPtr = (s16 *) osVirtualToPhysical(arg0->unk0); // audioPtr = (s16 *) osVirtualToPhysical(info->data);

    new_var = &D_global_asm_80770558;
    func_global_asm_80602314();
    var_v1 = osAiGetLength() >> 2;
    if (lastInfo) {
        s32 frameSamples = lastInfo->unk4 << 2;
        osAiSetNextBuffer(lastInfo->unk0, frameSamples); // outputDataPointer, frameSamples
    }
    if ((var_v1 < 0x5C) && (*new_var == 0)) {
        arg0->unk4 = D_global_asm_8077019C;
        D_global_asm_80770558 = 2;
    } else {
        temp = *new_var;
        if ((var_v1 >= 0x115) && (temp == 0)) {
            arg0->unk4 = D_global_asm_80770194;
            D_global_asm_80770558 = 2;
        } else {
            arg0->unk4 = 0x2E0;
            if (temp != 0) {
                temp -= 1;
                D_global_asm_80770558 = temp;
            }
        }
    }

    temp_v1 = n_alAudioFrame((Acmd *) (&D_global_asm_8076D4D0)[D_global_asm_807452C8].unk0[0], &sp34, audioPtr, (s32) arg0->unk4);

    if (sp34 == 0) {
        return 0;
    }
    
    arg0->unk8 = 0; //  next
    arg0->unk5C = &D_global_asm_8076D6D0; // msgQ
    arg0->unk60 = (OSMesg) (&arg0->unk68); // msg
    arg0->unk10 = 1;
    arg0->unk58 = &D_global_asm_8076D4C0;
    arg0->unk48 = (Acmd *) (&D_global_asm_8076D4D0)[D_global_asm_807452C8].unk0[0];
    arg0->unk4C = (temp_v1 - ((Acmd *) (&D_global_asm_8076D4D0)[D_global_asm_807452C8].unk0[0])) * sizeof(Acmd); // t->list.t.data_size
    arg0->unk18 = 2; // t->list.t.flags = OS_TASK_DP_WAIT;
    arg0->unk20 = &D_805FB000;
    arg0->unk24 = (&D_805FB0D0) - (&D_805FB000); // t->list.t.ucode_boot_size = (s32) ((s32) rspbootTextEnd - (s32) rspbootTextStart);
    arg0->unk1C = 0;
    arg0->unk28 = &D_global_asm_80741310;
    arg0->unk30 = &D_global_asm_80760590;
    arg0->unk34 = SP_UCODE_DATA_SIZE; // t->list.t.ucode_data_size = SP_UCODE_DATA_SIZE;
    arg0->unk50 = 0;
    arg0->unk54 = 0x400;

    osWritebackDCacheAll();

    osSendMesg(
        (OSMesgQueue *) func_global_asm_8060EE58((s32) (&D_global_asm_80767A40)), // OSMesgQueue*
        &arg0->unk8, // OsMesg*
        0 // flags
    );

    D_global_asm_807452C8 ^= 1; // swap which acmd list you use each frame

    return 1;
}

extern void func_arcade_80026680(Gfx **dl_ptr);
extern void func_arcade_800268AC(Gfx **dl_ptr);
extern void func_arcade_80026EF4(Gfx **dl_ptr);
extern void func_arcade_800275E8(Gfx **dl_ptr);
extern void func_arcade_80027A38(Gfx **dl_ptr);
extern u8  D_arcade_8004C724;
extern s32 D_arcade_8004C6DC;
extern u8  arcade_background_visual;

// @recomp: Arcade DL Stuff
RECOMP_PATCH void func_arcade_800259D0(Gfx **arg0) {
    Gfx *dl = *arg0;
    gDPSetAlphaCompare(dl++, G_AC_NONE);
    gDPSetTexturePersp(dl++, G_TP_NONE);
    // @recomp: Remove texture filtering
    gDPSetTextureFilter(dl++, G_TF_POINT);
    gDPSetTextureConvert(dl++, G_TC_FILT);
    gDPSetTextureDetail(dl++, G_TD_CLAMP);
    gDPSetTextureLOD(dl++, G_TL_TILE);
    gDPSetTextureLUT(dl++, G_TT_NONE);
    gDPSetPrimColor(dl++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetRenderMode(dl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(dl++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, 48, 36, 272, 232);

    switch (D_arcade_8004C724) {
        case 0://L80025B68
            if (D_arcade_8004C6DC & 0x200) {
                func_arcade_80026680(&dl);
            }
            break;
        case 1:
        case 4:
        case 5://L80025B88
            switch (arcade_background_visual) {
                case 1:
                    func_arcade_800268AC(&dl);
                    break;
                case 2:
                    func_arcade_80026EF4(&dl);
                    break;
                case 3:
                    func_arcade_800275E8(&dl);
                    break;
                default:
                    func_arcade_80027A38(&dl);
                    break;
            }
            break;
    }
    *arg0 = dl;
}

// @recomp: (Prevent the ) draw of borders for overscan
RECOMP_PATCH Gfx *func_global_asm_80704960(Gfx *dl) {
    return dl;
}

typedef struct {
    f32 unk0;
    f32 unk4;
    f32 unk8;
    f32 unkC;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1C;
    f32 unk20;
    f32 unk24;
    f32 unk28;
    f32 unk2C;
} Struct807F6C14;
extern Struct807F6C14 *D_global_asm_807F6C14;
typedef struct {
    f32 unk0;
    f32 unk4;
} Struct807F6C88;
typedef struct {
    f32 unk0;
    f32 unk4;
    f32 unk8;
} Struct807F6D78;

extern Struct807F6C88 D_global_asm_807F6C88;
extern Struct807F6C88 D_global_asm_807F6C90;
extern Struct807F6C88 D_global_asm_807F6C98;
extern Struct807F6C88 D_global_asm_807F6CA0;
extern Struct807F6C88 D_global_asm_807F6CA8;
extern Struct807F6C88 D_global_asm_807F6CB0;
extern Struct807F6D78 D_global_asm_807F6F58;
extern Struct807F6D78 D_global_asm_807F6E68;
extern Struct807F6D78 D_global_asm_807F6D78[];
void func_global_asm_806582F8(Struct807F6D78 *, Struct807F6D78 *, s32, s32 *, s32);

// @recomp: Frustum Bound check
RECOMP_PATCH s32 func_global_asm_80658134(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    s32 sp28;
    s32 sp24;
    Struct807F6C14 *temp_v0;
    s32 var_a2;

    sp24 = 0;
    sp28 = 0;
    temp_v0 = &D_global_asm_807F6C14[arg0];
    D_global_asm_807F6C88.unk0 = temp_v0->unk0;
    D_global_asm_807F6C88.unk4 = temp_v0->unk10;
    D_global_asm_807F6C90.unk0 = temp_v0->unk20;
    D_global_asm_807F6C90.unk4 = temp_v0->unk4;
    D_global_asm_807F6C98.unk0 = temp_v0->unk14;
    D_global_asm_807F6C98.unk4 = temp_v0->unk24;
    D_global_asm_807F6CA0.unk0 = temp_v0->unk8;
    D_global_asm_807F6CA0.unk4 = temp_v0->unk18;
    D_global_asm_807F6CA8.unk0 = temp_v0->unk28;
    D_global_asm_807F6CA8.unk4 = temp_v0->unkC;
    D_global_asm_807F6CB0.unk0 = temp_v0->unk1C;
    D_global_asm_807F6CB0.unk4 = temp_v0->unk2C;

    // @recomp: Disable X Viewport checks with depth buffer checking
    func_global_asm_806582F8((Struct807F6D78 *)&D_global_asm_807F6C88, &D_global_asm_807F6E68, 4, &sp28, 0);
    if (sp28 == 0) return 0;
    _memcpy(&D_global_asm_807F6F58, &D_global_asm_807F6E68, sizeof(tuple_f) * sp28);
    sp24 = sp28;
    func_global_asm_806582F8(&D_global_asm_807F6F58, &D_global_asm_807F6E68, sp24, &sp28, 2);
    if (sp28 == 0) return 0;
    _memcpy(&D_global_asm_807F6F58, &D_global_asm_807F6E68, sizeof(tuple_f) * sp28);
    sp24 = sp28;
    func_global_asm_806582F8(&D_global_asm_807F6F58, D_global_asm_807F6D78, sp24, &sp28, 4);
    var_a2 = sp28;
    if (var_a2 < 3) {
        var_a2 = 0;
    }
    return var_a2;
}

// Used for chunk debugging
s32 func_global_asm_806574B8(s32);

typedef struct Struct807F6C1C {
    u8 pad0[0x2];
    s16 unk2;
    s16 unk4;
    s16 unk6;
    u8 unk8;
    u8 unk9;
} Struct807F6C1C;

extern Struct807F6C0C *D_global_asm_807F6C0C;
extern s32 D_global_asm_807F6C10;
extern Struct807F6C1C *D_global_asm_807F6C1C;
extern s32 D_global_asm_807F6C20;
extern u8 *D_global_asm_807F6C2C;
extern s16 D_global_asm_807F6C30[];
extern u8 D_global_asm_807F7078[];
extern u8 func_global_asm_806575D0(s32 arg0, f32 arg1, f32 arg2, f32 arg3);
extern void func_global_asm_8065756C(s16 arg0);
extern void func_global_asm_80657CB0(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 *arg4, f32 *arg5, f32 *arg6);
extern void func_global_asm_80657E24(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 *arg8, s32 *arg9, s32 *argA, s32 *argB);
extern s32 func_global_asm_80657F14(s32 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, s32 *arg8, s32 *arg9, s32 *argA, s32 *argB);
extern s32 func_global_asm_80655CF8(s16 arg0, s32 arg1);
extern u8 func_global_asm_80658000(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s16 arg4);
extern void func_global_asm_80658624(s32 arg0, s32 *arg1, s32 *arg2, s32 *arg3, s32 *arg4);
extern void func_global_asm_80657508(s32 arg0);
extern s32 func_global_asm_80663040(s32 arg0);

// @recomp: Chunk Renderer
RECOMP_PATCH void func_global_asm_80656F14(s16 arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, f32 arg6, f32 arg7, f32 arg8) {
    s32 temp_s1;
    s32 temp_v0;
    s32 var_s0;
    s32 spC8;
    s32 spC4;
    s32 spC0;
    s32 spBC;
    s32 spB8;
    s32 spB4;
    s32 spB0;
    s32 spAC;
    s32 spA8;
    s32 spA4;
    s32 spA0;
    s32 sp9C;
    u8 cond2;
    u8 cond3;
    s32 cond1;
    s32 var_s2;
    s32 var_v1;

    func_global_asm_8065756C(arg0);
    for (var_s2 = 0; var_s2 < D_global_asm_807F6C20; var_s2++) {
        if ((arg0 == D_global_asm_807F6C1C[var_s2].unk2) && (!func_global_asm_80655CF8(D_global_asm_807F6C1C[var_s2].unk4, arg1))) {
            if (D_global_asm_807F6C1C[var_s2].unk9) {
                D_global_asm_807F6C1C[var_s2].unk8 = 1;
                temp_s1 = var_s2 / 2;
                if (func_global_asm_806574B8(temp_s1)) {
                    cond3 = func_global_asm_806575D0(temp_s1, arg6, arg7, arg8);
                    if (cond3) {
                        spB0 = arg4;
                        spB8 = arg2;
                        spB4 = arg3;
                        spAC = arg5;
                    } else if (func_global_asm_80658000(temp_s1, arg6, arg7, arg8, D_global_asm_807F6C1C[var_s2].unk6)) {
                        cond1 = func_global_asm_80658134(temp_s1, arg6, arg7, arg8);
                        if (cond1) {
                            cond2 = TRUE;
                            spB0 = arg4;
                            spB8 = arg2;
                            spB4 = arg3;
                            spAC = arg5;
                            // func_global_asm_80658624(cond1, &spC8, &spC4, &spC0, &spBC);
                            // if (newly_pressed_input & 0x0800) {
                            //     recomp_printf("--------------\n");
                            //     recomp_printf("Chunk %d:\n", D_global_asm_807F6C1C[var_s2].unk4);
                            //     recomp_printf("Ch %d, %d, %d, %d\n", spC8, spC4, spC0, spBC);
                            //     recomp_printf("SC %d, %d, %d, %d\n", arg2, arg3, arg4, arg5);
                            // }
                            // cond2 = func_global_asm_80657F14(spC8, spC4, spC0, spBC, arg2, arg3, arg4, arg5, &spB8, &spB4, &spB0, &spAC);
                        }
                    } else {
                        cond1 = 0;
                        var_v1 = FALSE;
                        var_s0 = 0;
                        while (!var_v1 && (var_s0 < D_global_asm_807F6C10)) {
                            if ((var_s2 == D_global_asm_807F6C0C[var_s0].unk2) && (D_global_asm_807F6C0C[var_s0].unk0 == D_global_asm_807F6C1C[var_s2].unk4)) {
                                var_v1 = TRUE;
                            } else {
                                var_s0++;
                            }
                        }
                        if ((var_v1 != 0) && (func_global_asm_80658000(temp_s1, arg6, arg7, arg8, D_global_asm_807F6C0C[var_s0].unk4))) {
                            temp_v0 = func_global_asm_80658134(temp_s1, arg6, arg7, arg8);
                            if (temp_v0) {
                                // func_global_asm_80658624(temp_v0, &spC8, &spC4, &spC0, &spBC);
                                // if (func_global_asm_80657F14(spC8, spC4, spC0, spBC, arg2, arg3, arg4, arg5, &spB8, &spB4, &spB0, &spAC) != 0) {
                                    func_global_asm_80657508(var_s0);
                                // }
                            }
                        }
                        // if (func_global_asm_80663040(var_s2)) {
                        //     temp_v0 = func_global_asm_80658134(temp_s1, arg6, arg7, arg8);
                        //     if (temp_v0) {
                        //         func_global_asm_80658624(temp_v0, &spC8, &spC4, &spC0, &spBC);
                        //         if (func_global_asm_80657F14(spC8, spC4, spC0, spBC, arg2, arg3, arg4, arg5, &spB8, &spB4, &spB0, &spAC)) {
                        //             D_global_asm_807F6C2C[var_s2] = 1;
                        //         }
                        //     }
                        // }
                    }
                    if (((cond1) && (cond2)) || (cond3)) {
                        D_global_asm_807F6C2C[var_s2] = 1;
                        temp_s1 = D_global_asm_807F6C1C[var_s2].unk4;
                        if (D_global_asm_807F7078[temp_s1]) {
                            chunk_array_pointer[temp_s1].loaded = 1;
                            chunk_array_pointer[temp_s1].unk1 |= 1 << cc_player_index;
                            func_global_asm_80657E24(spB8, spB4, spB0, spAC,
                                chunk_array_pointer[temp_s1].deload1,
                                chunk_array_pointer[temp_s1].deload2,
                                chunk_array_pointer[temp_s1].deload3,
                                chunk_array_pointer[temp_s1].deload4,
                                &spA8, &spA4, &spA0, &sp9C);
                            chunk_array_pointer[temp_s1].deload1 = spA8;
                            chunk_array_pointer[temp_s1].deload2 = spA4;
                            chunk_array_pointer[temp_s1].deload3 = spA0;
                            chunk_array_pointer[temp_s1].deload4 = sp9C;
                            D_global_asm_807F6C30[arg1] = arg0;
                            func_global_asm_80656F14(temp_s1, arg1 + 1, spB8, spB4, spB0, spAC, arg6, arg7, arg8);
                        }
                    }
                }
            }
        }
    }
}

typedef struct Struct807F93F4 {
    f32 unk0;
    f32 unk4;
    f32 unk8;
    f32 unkC;
    u8 unk10;
    u8 unk11;
    s8 unk12;
    s8 unk13;
} Struct807F93F4;

typedef struct Struct807F93F0 {
    u8 pad0[0x38];
    s16 unk38;
    s16 unk3A;
    s16 unk3C;
    u8 unk3E;
    u8 unk3F;
} Struct807F93F0;

extern u8 *D_global_asm_807F6C2C;
extern Struct807F93F0 *D_global_asm_807F93F0;
extern Struct807F93F4 *D_global_asm_807F93F4;
extern s32 D_global_asm_807F93F8;
extern s16 func_global_asm_80665DE0(f32 arg0, f32 arg1, f32 arg2, f32 arg3);
extern f32 func_global_asm_80611BB4(f32 arg0, f32 arg1);

// @recomp: Distant Screen renderer
RECOMP_PATCH void func_global_asm_8066308C(f32 arg0, f32 arg1, f32 arg2) {
    f32 temp_f12;
    f32 delta;
    s32 i;

    for (i = 0; i < D_global_asm_807F93F8; i++) {
        D_global_asm_807F93F4[i].unk11 = 0;
        // if (D_global_asm_807F6C2C[D_global_asm_807F93F0[i].unk38]) {
            D_global_asm_807F93F4[i].unk0 = _sqrtf(SQ(arg0 - D_global_asm_807F93F4[i].unk4) + SQ(arg1 - D_global_asm_807F93F4[i].unk8) + SQ(arg2 - D_global_asm_807F93F4[i].unkC));
            temp_f12 = (D_global_asm_807F93F4[i].unk0 - D_global_asm_807F93F0[i].unk3A) / (D_global_asm_807F93F0[i].unk3C - D_global_asm_807F93F0[i].unk3A);
            if (temp_f12 > 1.0) {
                D_global_asm_807F93F4[i].unk10 = D_global_asm_807F93F0[i].unk3F;
            } else if (temp_f12 < 0.0) {
                D_global_asm_807F93F4[i].unk10 = D_global_asm_807F93F0[i].unk3E;
            } else {
                delta = D_global_asm_807F93F0[i].unk3F - D_global_asm_807F93F0[i].unk3E;
                delta *= temp_f12;
                D_global_asm_807F93F4[i].unk10 = delta + D_global_asm_807F93F0[i].unk3E;
            }
            D_global_asm_807F93F4[i].unk12 = (s32) (func_global_asm_80665DE0(arg0, arg2, D_global_asm_807F93F4[i].unk4, D_global_asm_807F93F4[i].unkC) * 0.5) % 255;
            D_global_asm_807F93F4[i].unk13 = ((s32) ((s16)((func_global_asm_80611BB4(D_global_asm_807F93F4[i].unk8 - arg1, _sqrtf(SQ(arg2 - D_global_asm_807F93F4[i].unkC) + SQ(arg0 - D_global_asm_807F93F4[i].unk4))) * 2048.0) / 3.1415927410125732) * 0.5) % 255);
            D_global_asm_807F93F4[i].unk11 = 1;
        // } else {
        //     D_global_asm_807F93F4[i].unk11 = 0;
        // }
    }
}

Gfx* func_global_asm_80703AB0(Gfx*, u8);
Gfx* func_global_asm_80703CF8(Gfx*, u8);
void func_global_asm_8070B324(f32, s32, s32 *);
u8 isIntroStoryPlaying(void);
void func_global_asm_80703850(u8 arg0);
u8 func_global_asm_805FF000(u8 map);
u8 func_global_asm_80600454(s16 arg0, u8 *arg1);
void func_global_asm_8069D2AC(u8 arg0, s16 arg1, s16 arg2, char *arg3, u16 arg4, u16 arg5, u8 arg6, u8 arg7);
u8 *getTextString(u8 fileIndex, s32 stringIndex, s32 arg2);
void func_global_asm_80602B60(s32 arg0, u8 arg1);
Gfx *func_global_asm_80703374(Gfx *dl, u8 r, u8 g, u8 b, u8 a);

extern u8 D_global_asm_80750AC0;
extern s32 D_global_asm_80754CC8[];
extern s16 D_global_asm_8076A0AA;
extern u8 D_global_asm_8076A0B1;
extern f32 D_global_asm_807FD888;
extern f32 loading_zone_transition_speed;
extern u8 D_global_asm_8076A0AB;

// @recomp: Transition manager
RECOMP_PATCH Gfx* func_global_asm_80704484(Gfx *dl, u8 arg1) {
    f32 temp_f0;
    u8 sp5B;
    u16 var_v1;
    f32 sp54;
    s32 var_v0;

    if (loading_zone_transition_speed != 0.0f) {
        if (arg1 == 2) {
            var_v0 = 2;
        } else {
            var_v0 = 1;
        }
        D_global_asm_807FD888 += loading_zone_transition_speed * var_v0;
        if (D_global_asm_807FD888 < 0.0f) {
            func_global_asm_805FF000(D_global_asm_8076A0AB);
            if (((D_global_asm_8076A0B1 & 0x20) == 0) && (func_global_asm_80600454(D_global_asm_8076A0AA, &sp5B))) {
                func_global_asm_8069D2AC(1, 0, 0x64, getTextString(0x23, sp5B, 1), 0, 0x28, 6, 8);
                D_global_asm_8076A0B1 |= 0x20;
            }
            func_global_asm_80602B60(0x2B, 0);
            if (isIntroStoryPlaying() == 2) {
                setIntroStoryPlaying(0);
            }
            loading_zone_transition_speed = 0.0f;
            D_global_asm_807FD888 = 0.0f;
            D_global_asm_8076A0B1 &= ~0x40;
        } else if (D_global_asm_807FD888 > 31.0f) {
            if (D_global_asm_8076A0B1 & 1) {
                func_global_asm_80602B60(0x2C, 0);
            }
            loading_zone_transition_speed = 0.0f;
            D_global_asm_807FD888 = 31.0f;
            osViBlack(1);
            D_global_asm_8076A0B1 |= 0xC;
            dl = func_global_asm_80703374(dl, 0, 0, 0, 0xFF);
        } else {
            sp54 = D_global_asm_807FD888 * 0.032258064f;

            temp_f0 = (D_global_asm_807FD888 * 255.0f) / 28.0f;
            var_v1 = (temp_f0 < 0.0f) ? 0.0f : MIN(255.0f, (temp_f0));
            
            if ((D_global_asm_807FD888 < ((loading_zone_transition_speed * 2) + 31.0f)) && (D_global_asm_8076A0B1 & 4)) {
                D_global_asm_8076A0B1 ^= 6;
            }
            if ((D_global_asm_807FD888 > 28.0f) && (arg1 != 3)) {
                dl = func_global_asm_80703374(dl, 0, 0, 0, 0xFF);
            } else {
                switch (arg1) {
                    case 3:
                        if (D_global_asm_80750AC0 == 1) {
                            func_global_asm_80703850(var_v1);
                            break;
                        }
                    case 2:
                        dl = func_global_asm_80703AB0(dl, var_v1);
                        break;
                    case 1: // Spin transition
                        // @recomp: Spin transition fix for non-overscan
                        if (D_global_asm_807FD888 > 25.0f) { // Spin progress
                            // Render nothing but black
                            dl = func_global_asm_80703374(dl, 0, 0, 0, 0xFF);
                        } else {
                            // Render spin
                            dl = func_global_asm_80703CF8(dl, var_v1);
                        }
                        break;
                    default:
                        dl = func_global_asm_80703374(dl, 0, 0, 0, var_v1);
                        break;
                }
            }
            if ((D_global_asm_8076A0B1 & 0x41) || !(D_global_asm_8076A0B1 & 0x20)) {
                s32 sp44[3] = { 0 };
                s32 pad;
                func_global_asm_8070B324(1.0f - sp54, loading_zone_transition_speed >= 0.0f, sp44);
            }
        }
    } else if (D_global_asm_807FD888 > 28.0f) {
        // @recomp: Fix a bug with mid-transition
        dl = func_global_asm_80703374(dl, 0, 0, 0, 0xFF);
    }
    return dl;
}

typedef struct {
    f32 unk0;
    f32 unk4;
    f32 unk8;
    f32 unkC;
    f32 unk10;
    f32 unk14;
    f32 unk18;
    f32 unk1C;
    f32 unk20;
    f32 unk24;
    f32 unk28;
    f32 unk2C;
    s16 unk30;
    s16 unk32;
    s16 unk34;
    u8 unk36;
    u8 unk37;
    u8 unk38;
    u8 unk39;
    s8 unk3A;
    s8 unk3B;
} Struct807F7500;

#define LIGHT_LIMIT 0x30
Struct807F7500 lightarray[LIGHT_LIMIT];
extern f32 D_global_asm_807F7ED0;
extern f32 D_global_asm_807F7ED4;
extern f32 D_global_asm_807F7ED8;
extern f32 D_global_asm_807F7EDC;
extern f32 D_global_asm_807F7EE0;
extern f32 D_global_asm_807F7EE4;
extern s16 D_global_asm_80748300;

extern f32 *D_global_asm_8076A0B4;
extern f32 *D_global_asm_8076A0B8;
extern f32 *D_global_asm_8076A0BC;
extern f32 *D_global_asm_8076A0C0;
extern f32 *D_global_asm_8076A0C4;
extern f32 *D_global_asm_8076A0C8;

extern s32 D_global_asm_807F6C28;
extern s16 D_global_asm_807F7EC8;

extern u8 D_global_asm_807F7EF8;
extern f32 D_global_asm_807F7ECC;
extern f32 D_global_asm_807F7ED0;
extern f32 D_global_asm_807F7EE8;
extern s16 D_global_asm_807F7EFA;
extern s16 D_global_asm_807F7EFC;
extern s16 D_global_asm_807F7EFE;

RECOMP_PATCH void createLight(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, u8 arg7, u8 arg8, u8 arg9, u8 argA) {
    if (D_global_asm_80748300 == LIGHT_LIMIT) {
        recomp_printf("Too many lights\n");
        return;
    }
    lightarray[D_global_asm_80748300].unk34 = D_global_asm_807F7EFE;
    lightarray[D_global_asm_80748300].unk3A = D_global_asm_807F7EF8;
    lightarray[D_global_asm_80748300].unk30 = D_global_asm_807F7EFA;
    lightarray[D_global_asm_80748300].unk32 = D_global_asm_807F7EFC;
    lightarray[D_global_asm_80748300].unk8 = D_global_asm_807F7EE8;
    lightarray[D_global_asm_80748300].unk0 = D_global_asm_807F7EE4;
    lightarray[D_global_asm_80748300].unk18 = arg0;
    lightarray[D_global_asm_80748300].unk1C = arg1;
    lightarray[D_global_asm_80748300].unkC = D_global_asm_807F7EDC;
    lightarray[D_global_asm_80748300].unk4 = D_global_asm_807F7ED8;
    lightarray[D_global_asm_80748300].unk39 = arg7;
    lightarray[D_global_asm_80748300].unk10 = D_global_asm_807F7EE0;
    lightarray[D_global_asm_80748300].unk20 = arg2;
    lightarray[D_global_asm_80748300].unk24 = arg3;
    lightarray[D_global_asm_80748300].unk28 = arg4;
    lightarray[D_global_asm_80748300].unk2C = arg5;
    lightarray[D_global_asm_80748300].unk14 = arg6;
    lightarray[D_global_asm_80748300].unk36 = arg8;
    lightarray[D_global_asm_80748300].unk37 = arg9;
    lightarray[D_global_asm_80748300].unk38 = argA;
    D_global_asm_80748300++;
    D_global_asm_807F7ED8 = D_global_asm_807F7ECC;
    D_global_asm_807F7EDC = D_global_asm_807F7ED0;
    D_global_asm_807F7EE0 = D_global_asm_807F7ED4;
    D_global_asm_807F7EF8 = 0;
    D_global_asm_807F7EFA = -1;
    D_global_asm_807F7EFC = 700;
    D_global_asm_807F7EFE = 600;
    D_global_asm_807F7EE4 = 25.0f;
    D_global_asm_807F7EE8 = 65.0f;
}

typedef struct {
    Actor* unk0;
    s32 unk4;
} GlobalASMStruct53;
s32 func_global_asm_8065C240(void*);
void addActorRecolor(Actor *actor, s16 x, s16 y, s16 z, u8 alpha, u8 red, u8 green, u8 blue, u8);
void func_global_asm_8065D254(Actor *actor, s32 arg1, s32 arg2, s32 arg3, s32 arg4, s32 arg5, s32 arg6, s32 arg7, u32 arg8, s32 arg9, f32 arg10);
extern s32 D_global_asm_80748304;
extern s32 D_global_asm_80748308;
extern u16 D_global_asm_807FBB34;
extern GlobalASMStruct53 D_global_asm_807FB930[];
#define drawShadow8065A884(m) \
    temp_res = ac->shadow_opacity / 255.0; \
    func_global_asm_8065D254(ac, \
    0x3C9, 0x40, 0x40, \
    spC0 * ac->unk16E, spC4 * ac->unk16F, \
    1, 0x64, \
    (temp_res * D_global_asm_80748304 * (m)), \
    var_s3 + 0xC00, var_f24 * (m))

RECOMP_PATCH void func_global_asm_8065A884(void) {
    s32 i;
    Struct807F7500* light;
    s32 sp14C;
    f32 temp_f14;
    f32 temp_f2;
    f32 temp_f0;
    f32 temp_f0_10;
    f32 temp_f0_11;
    f32 temp_f0_6;
    f32 temp_f12;
    f32 temp_f16;
    f32 temp_f18;
    f32 temp_f20;
    f32 temp_f26;
    f32 temp_f28;
    f32 temp_f2_5;
    f32 temp_f30;
    f32 var_f20;
    f32 var_f24; // 10c
    s16 var_s3;
    u8 spC8[0x40];
    f32 spC4;
    f32 spC0;
    Actor* ac;
    f32 var_f20_2;
    f32 var_f22;
    f32 var_f2;
    s32 j;
    f32 temp_res;

    sp14C = 0;
    for (i = 0; i < D_global_asm_807FBB34; i++) {
        spC8[i] = FALSE;
    }
    for (i = 0; (i < D_global_asm_80748300) && (cc_number_of_players < 3); i++) {
        light = &lightarray[i];
        if ((func_global_asm_8065C240(light)) && (sp14C < 0xC)) {
            sp14C += 1;
            for (j = 0; j < D_global_asm_807FBB34; j++) {
                ac = D_global_asm_807FB930[j].unk0;
                if ((ac->object_properties_bitfield & 0x2000) == 0) {
                    switch (light->unk39) {
                        case 1:
                            temp_f0 = light->unk24 - light->unk18;
                            temp_f2 = light->unk28 - light->unk1C;
                            temp_f14 = light->unk2C - light->unk20;
                            var_f20 = _sqrtf(
                                SQ(temp_f0) + 
                                SQ(temp_f2) + 
                                SQ(temp_f14));
                            if (var_f20 == 0.0f) {
                                var_f20 = 0.1f;
                            }
                            temp_f26 = temp_f0 / var_f20;
                            temp_f28 = temp_f2 / var_f20;
                            temp_f30 = temp_f14 / var_f20;
                            temp_f0 = ac->position.f[0] - light->unk18;
                            temp_f2 = ac->position.f[1] - light->unk1C;
                            temp_f14 = ac->position.f[2] - light->unk20;
                            var_f20_2 = _sqrtf(
                                SQ(temp_f0) + 
                                SQ(temp_f2) + 
                                SQ(temp_f14)
                            );
                            if (var_f20_2 == 0.0f) {
                                var_f20_2 = 0.1f;
                            }
                            temp_f12 = temp_f0 / var_f20_2;
                            temp_f16 = temp_f2 / var_f20_2;
                            temp_f18 = temp_f14 / var_f20_2;
                            var_f22 = (temp_f12 * temp_f26) + (temp_f16 * temp_f28) + (temp_f18 * temp_f30);
                            if (var_f22 < 0.0f) {
                                var_f22 = 0.0f;
                            }
                            if (light->unkC <= var_f22) {
                                if (light->unk4 <= var_f22) {
                                    var_f22 = 1.0f;
                                } else {
                                    var_f22 = (var_f22 - light->unkC) * light->unk10;
                                }
                                addActorRecolor(ac,
                                    light->unk18,
                                    light->unk1C,
                                    light->unk20, 0xFF,
                                    light->unk36 * var_f22,
                                    light->unk37 * var_f22,
                                    light->unk38 * var_f22, 0U);
                                if ((ac->object_properties_bitfield & 0x01000000) && (ac->object_properties_bitfield & 4)) {
                                    temp_f0_6 = _sqrtf(
                                        SQ(light->unk20 - ac->position.f[2]) + 
                                        SQ(light->unk18 - ac->position.f[0])
                                    );
                                    if (temp_f0_6 != 0.0f) {
                                        var_f2 = temp_f0_6;
                                    } else {
                                        var_f2 = 0.001f;
                                    }
                                    var_f24 = (light->unk1C - ac->position.f[1]) / var_f2;
                                    if (!(var_f24 > 0.0f)) {
                                        var_f24 = -var_f24;
                                    }
                                    if (var_f24 == 0.0f) {
                                        var_f24 = 0.1f;
                                    }
                                    var_f24 = (ac->unk15E * 0.4f) / var_f24;
                                    var_s3 = func_global_asm_80665DE0(
                                        light->unk18, light->unk20,
                                        ac->position.f[0], ac->position.f[2]
                                    );
                                    if (ac->animation_state) {
                                        spC0 = ac->animation_state->scale[0];
                                        spC4 = ac->animation_state->scale[2];
                                    } else {
                                        spC0 = 0.15f;
                                        spC4 = 0.15f;
                                    }
                                    temp_res = ac->shadow_opacity / 255.0;
                                    func_global_asm_8065D254(ac,
                                        0x3C9, 0x40, 0x40,
                                        spC0 * ac->unk16E, spC4 * ac->unk16F,
                                        1, 0x64,
                                        (D_global_asm_80748304 * var_f22 * temp_res),
                                        var_s3 + 0xC00, var_f24 * var_f22);
                                    spC8[j] = TRUE;
                                }
                            }
                            break;
                        case 0:
                            var_f22 = _sqrtf(
                                SQ(light->unk18 - ac->position.f[0]) +
                                SQ(light->unk1C - ac->position.f[1]) + 
                                SQ(light->unk20 - ac->position.f[2])
                            ) - (f32) ac->unkCE;
                            if (var_f22 < 0.0) {
                                var_f22 = 0.1f;
                            }
                            temp_f20 = light->unk14 * 1.3f;
                            if ((ac->object_properties_bitfield & 0x01000000) && (var_f22 < temp_f20) && (ac->object_properties_bitfield & 4)) {
                                var_f2 = _sqrtf(
                                    SQ(light->unk20 - ac->position.f[2]) +
                                    SQ(light->unk18 - ac->position.f[0])
                                );
                                if (var_f2 == 0.0f) {
                                    var_f2 = 1.0f;
                                }
                                var_f24 = (light->unk1C - ac->position.f[1]) / var_f2;
                                if (!(var_f24 > 0.0f)) {
                                    var_f24 = -var_f24;
                                }
                                if (var_f24 == 0.0f) {
                                    var_f24 = 0.1f;
                                }
                                var_f24 = (ac->unk15E * 0.4f) / var_f24;
                                var_s3 = func_global_asm_80665DE0(light->unk18, light->unk20, ac->position.f[0], ac->position.f[2]);
                                if (var_f24 > 10.0f) {
                                    var_f24 = 10.0f;
                                }
                                if (ac->animation_state) {
                                    spC0 = ac->animation_state->scale[0];
                                    spC4 = ac->animation_state->scale[2];
                                } else {
                                    spC0 = 0.15f;
                                    spC4 = 0.15f;
                                }
                                spC8[j] = 1;
                            }
                            temp_f0_10 = temp_f20 / 3.0f;
                            if (var_f22 < temp_f0_10) {
                                addActorRecolor(ac,
                                    light->unk18,
                                    light->unk1C,
                                    light->unk20, 0xFFU,
                                    light->unk36,
                                    light->unk37,
                                    light->unk38, 0U);
                                if ((ac->object_properties_bitfield & 0x01000000) && (ac->object_properties_bitfield & 4)) {
                                    drawShadow8065A884(1.0f);
                                }
                            } else {
                                var_f22 -= temp_f0_10;
                                temp_f2_5 = temp_f20 - temp_f0_10;
                                if (var_f22 < temp_f2_5) {
                                    temp_f0_11 = 1.0f - (var_f22 / temp_f2_5);
                                    addActorRecolor(ac,
                                        light->unk18,
                                        light->unk1C,
                                        light->unk20, 0xFFU,
                                        light->unk36 * temp_f0_11,
                                        light->unk37 * temp_f0_11,
                                        light->unk38 * temp_f0_11, 0U);
                                    if ((ac->object_properties_bitfield & 0x01000000) && (ac->object_properties_bitfield & 4)) {
                                        drawShadow8065A884(temp_f0_11);
                                    }
                                }
                            }
                            break;
                    }
                }
            }
        }
    }
    for (i = 0; i < D_global_asm_807FBB34; i++) {
        if (spC8[i] == 0) {
            ac = D_global_asm_807FB930[i].unk0;
            if ((ac->object_properties_bitfield & 0x01000000) && (ac->object_properties_bitfield & 4)) {
                if (ac->animation_state) {
                    spC0 = ac->animation_state->scale[0];
                    spC4 = ac->animation_state->scale[2];
                } else {
                    spC0 = 0.15f;
                    spC4 = 0.15f;
                }
                temp_res = ac->shadow_opacity / 255.0;
                func_global_asm_8065D254(ac,
                    0x3C9, 0x40, 0x40,
                    spC0 * ac->unk16E, spC4 * ac->unk16F,
                    1, 0x64,
                    (D_global_asm_80748308 * temp_res),
                    0, 1.0f);
            }
        }
    }
}

typedef struct {
    f32 unk0;
    f32 unk4;
    f32 unk8;
    f32 unkC;
    f32 unk10;
    f32 unk14;
    s32 unk18;
    f32 unk1C;
    f32 unk20;
    f32 unk24;
    f32 unk28;
    f32 unk2C;
    f32 unk30;
    f32 unk34;
    s16 unk38;
    s16 unk3A;
    s16 unk3C;
    u8 unk3E;
    u8 unk3F; // Used
} Struct807F78C0;
s32 func_global_asm_8065BF18(Struct807F7500*, f32, f32, s32, s32, s32, s32, s32, s32, s32, s32);
void func_global_asm_8065BE74(s16 arg0);
extern Struct807F78C0 D_global_asm_807F78C0[];
extern s16 D_global_asm_807F7BC0;

RECOMP_PATCH s16 func_global_asm_8065BAA0(f32 arg0, f32 arg1, s32 arg2, s16 arg3, s16 arg4, s16 arg5, s16 arg6, s16 arg7, s16 arg8, s32 arg9, u8* argA) {
    f32 dz;
    f32 dy;
    f32 dx;
    f32 var_f2;
    s32 i;

    *argA = FALSE;
    D_global_asm_807F7BC0 = 0;
    for (i = 0; i < D_global_asm_80748300; i++) {
        if (func_global_asm_8065BF18(&lightarray[i], arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)) {
            if (D_global_asm_807F7BC0 < 0xC) {
                if (lightarray[i].unk3A & 1) {
                    *argA = TRUE;
                }
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk3F = i;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk38 = lightarray[i].unk18 * 3.0f;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk3A = lightarray[i].unk1C * 3.0f;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk3C = lightarray[i].unk20 * 3.0f;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk2C = lightarray[i].unk36 / 255.0;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk30 = lightarray[i].unk37 / 255.0;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk34 = lightarray[i].unk38 / 255.0;
                D_global_asm_807F78C0[D_global_asm_807F7BC0].unk3E = lightarray[i].unk39;
                switch (lightarray[i].unk39) {
                    case 1:
                        dx = lightarray[i].unk24 - lightarray[i].unk18;
                        dy = lightarray[i].unk28 - lightarray[i].unk1C;
                        dz = lightarray[i].unk2C - lightarray[i].unk20;
                        var_f2 = _sqrtf(SQ(dx) + SQ(dy) + SQ(dz));
                        if (var_f2 == 0.0f) {
                            var_f2 = 0.1f;
                        }
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk18 = 1210000;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk4 = lightarray[i].unk4;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unkC = dx / var_f2;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk0 = lightarray[i].unkC;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk10 = dy / var_f2;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk8 = lightarray[i].unk10;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk14 = dz / var_f2;
                        break;
                    case 0:
                        if (lightarray[i].unk14 == 0.0f) {
                            lightarray[i].unk14 = 0.1f;
                        }
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk20 = ((lightarray[i].unk14 / 3.0f) * 3.0f);
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk18 = SQ(lightarray[i].unk14 * 3.0f);
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk1C = 1.0f / ((lightarray[i].unk14 * 3.0f) - D_global_asm_807F78C0[D_global_asm_807F7BC0].unk20);
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk28 = ((lightarray[i].unk14 * 1.3f) / 3.0f) * 3.0f;
                        D_global_asm_807F78C0[D_global_asm_807F7BC0].unk24 = 1.0f / (((lightarray[i].unk14 * 1.3f) * 3.0f) - D_global_asm_807F78C0[D_global_asm_807F7BC0].unk28);
                        break;
                }
                func_global_asm_8065BE74(D_global_asm_807F7BC0);
                D_global_asm_807F7BC0++;
            }
        }
    }
    return D_global_asm_807F7BC0;
}
// ---- Story co-op: replacement of func_global_asm_806C7D40 (character_change_array setup) ----
// Identical to the original except that the Kong Battle controller actor (ACTOR_UNKNOWN_318,
// whose script lives in the multiplayer overlay, not loaded in story maps) is not spawned in co-op.
extern u8 cc_player_index;
extern CharacterChange *character_change_array;
extern Actor *gCurrentPlayer;
extern Actor *gPlayerPointer;
extern f32 D_global_asm_807444C4;
extern s8 D_global_asm_80750AD0;
extern OSContPad D_global_asm_807ECD10[];
extern u16 D_global_asm_807FD570;
extern s32 D_global_asm_807FBB68;
extern u8 D_global_asm_807FC950[]; // PlayerProgress[], stride 0x306
#define COOP_PP_HEALTH(i) ((s8)D_global_asm_807FC950[(i) * 0x306 + 0x2FB])
#define COOP_PP_2FD(i) ((s8)D_global_asm_807FC950[(i) * 0x306 + 0x2FD])
typedef struct {
    u8 unk0; u8 unk1; u8 unk2; u8 unk3;
    s16 unk4; s16 unk6; s16 unk8; s16 unkA;
} CoopVpEntry;
extern s32 spawnActor(Actors, s32);
extern u8 current_character_index[];
extern u16 newly_pressed_input[]; // u16 per player (2-byte stride), NOT OSContPad
extern void *_malloc(s32);
extern void func_global_asm_80611690(void *);
extern void func_global_asm_806C850C(s32, u8);
extern CoopVpEntry *func_global_asm_806C7C94(u8);
static s16 coop_vp_rect[2][4];
static f32 coop_vp_asp[2];
static int coop_vp_saved = 0;
extern void func_global_asm_80708BB8(CharacterChange2DC *);
extern void func_global_asm_80709464(s32);
extern void func_global_asm_806C80E4(void);

// Kong played by P2, remembered across map loads (0xFF = none: first spawn after loading the game)
static u8 coop_p2_kong_mem = 0xFF;
// Training Grounds: P2 stays where he is while P1 visits Cranky's lab / a barrel (separate maps).
// His position is saved when P1 leaves the map and given back (standing, idle) when the map is loaded again.
static int coop_tg_free = 0;   // intro over / P2 no longer glued to P1
// Bonus barrel (mini-game map = single player map): the Kong who entered the barrel plays it.
static int coop_mg_pending = 0;  // P2 entered a barrel: the next single-player map is played by P2's Kong with controller 2
static int coop_mg_swapped = 0;  // P2's persona (Kong, ammo, health...) currently sits in the player 0 slot
static int coop_mg_col = 0;      // colour index of that Kong
static int coop_mg_ret = 0;      // frames left during which the map scripts run for P2 (he comes back out of the barrel)
static u32 coop_door_trig_t = 0; // frame (coop_gun_clock) at which the door trigger was set
static int coop_door_trig = -1; // world door swirl running (both Kongs in state 135): player who walked into it
static int coop_door_p2 = 0;    // that player was P2 when the map changed
static int coop_door_hs = 0; // the door being used leads to / from Troff 'n' Scoff (Hott&Stroff)
static s16 coop_door_ang; // angle of the exit
static f32 coop_door_ex, coop_door_ez; // exit point of the map being entered (the door)
extern f32 func_global_asm_80612794(s16);
extern f32 func_global_asm_80612790(s16);
static int coop_arena_was = 0, coop_arena_back = 0; // previous map was a crown arena / ticks left to put P2 beside P1 after the return
static int coop_door_ret1 = 0;  // same for P1 (the other Kong is then the one kept beside the walker)
static int coop_door_p1 = 0;
#define COOP_ENTRY_CENTER 12.5f // the Kong who walks in is moved this far to the right of the exit point (the other one stays COOP_ENTRY_SIDE at his left)
#define COOP_HS_AHEAD 50.0f // Hott&Stroff exit: distance in front of the door at which the other Kong is put (if he ends up behind the door, use a negative value)
#define COOP_HS_TURN 0 // heading of the other Kong relative to the exit angle (0x800 = turned around)
#define COOP_HS_EXIT (coop_door_hs && current_map != MAP_TROFF_N_SCOFF && coop_door_ret1 > 0) // arrival in a world after leaving Troff 'n' Scoff (Hott&Stroff exit)

static int coop_tg_map = MAP_TRAINING_GROUNDS; // map in which the Kong who stays is held (Training Grounds, or DK Isles for the shops / labs)
static int coop_tg_stayer = 1;   // player who stays in the Training Grounds while the other plays the barrel
// Arrival scenes with story cutscenes: Training Grounds -> DK Isles (parrot, then the cutscene showing the next place to go) and
// DK Isles -> Klumsy (first visit: roll, Klumsy talks, opens the first world's hall). The Kong who walked in (coop_isles_who)
// keeps the full screen, the speech and the camera until the last cutscene is over; then the split screen fades back.
static int coop_isles_scene = 0;
static int coop_isles_map = 0;  // map the scene happens in (destination of the zone)
static int coop_isles_from = 0; // map the player came from
static int coop_isles_trip = 0; // the scene is Klumsy's sequence (it may go to DK Isles and back, longer pauses)
static int coop_cs_trig = -1;   // Japes lobby, B. Locker cutscene (camera focus + text): Kong who triggered it (0/1), -1 if none; nobody is teleported to the other
static int coop_tg_cave = 0;    // arriving in the Training Grounds by the cave (from DK Isles): P1 walks, P2 keeps P1's heading during the walk
static int coop_tg_cave_on = 0; // the walk of that entry has started
static int coop_jp18 = 0;      // Japes arrival cutscene (18) has played and its speech bubble may still be shown: P1's full screen, pads locked until it ends
static int coop_txt_last = -100; // last frame a speech bubble was updated (the parrot's last lines outlast the cutscene)
static int coop_glue_side = 0;  // P1 ran a forced walk during this cutscene: P2 is kept at his side (see the glue at the end of func_global_asm_8067889C)
static int coop_isles_cs = 0;   // a real cutscene has played in this scene (the first visit): long pauses are expected before the hall scene
static int coop_isles_ret = 0;  // back in Klumsy after the hall scene: only the last text and the walk remain, no long pause
static int coop_isles_who = 0;
static int coop_isles_seen = 0; // a cutscene / walk has been seen since the arrival
static int coop_isles_idle = 0; // frames without cutscene nor walk
static int coop_isles_t = 0;    // frames since the arrival
static void coop_mg_swap_persona(void) {
    u8 t = current_character_index[0];
    int i;
    current_character_index[0] = current_character_index[1];
    current_character_index[1] = t;
    for (i = 0x2F0; i < 0x2FE; i++) {
        t = D_global_asm_807FC950[i];
        D_global_asm_807FC950[i] = D_global_asm_807FC950[0x306 + i];
        D_global_asm_807FC950[0x306 + i] = t;
    }
}
extern f32 func_global_asm_80612794(s16);
extern f32 func_global_asm_80612790(s16);
static int coop_active_here(void);
static OSContPad coop_zero_pad; // neutral pad
extern Actor *D_global_asm_807FD710; // actor that follows the autowalk path
// Player who walks into a loading zone: P2 when he is the one who triggered it (a single-player map follows: lab, shop...)
extern Actor *gPlayerPointer;
extern s16 D_global_asm_807FDCB8;
extern s16 D_global_asm_807FDCBC;
static int coop_dbg_st[2]; // control state of both Kongs (tracked each frame)
Actor *coop_zone_player(void) {
    if (coop_mg_pending && coop_active_here() && character_change_array != NULL && character_change_array[1].does_player_exist
        && character_change_array[1].playerPointer != NULL) return character_change_array[1].playerPointer;
    // (gPlayerPointer is the Kong nearest to the zone, not necessarily the one who walked into it: when P1 entered, P1 walks)
    if (coop_active_here() && current_map == MAP_TRAINING_GROUNDS && character_change_array != NULL && character_change_array[0].playerPointer != NULL)
        return character_change_array[0].playerPointer;
    // elsewhere: the Kong that is running into the zone (state 13) while the other one stands, else the nearest one
    if (coop_active_here() && character_change_array != NULL && character_change_array[1].does_player_exist
        && character_change_array[0].playerPointer != NULL && character_change_array[1].playerPointer != NULL) {
        if (coop_dbg_st[1] == 13 && coop_dbg_st[0] != 13) return character_change_array[1].playerPointer;
        if (coop_dbg_st[0] == 13 && coop_dbg_st[1] != 13) return character_change_array[0].playerPointer;
        if (coop_dbg_st[0] == 13 && coop_dbg_st[1] == 13) {
            // both running: the one closest to the zone (its position is set before the walk starts)
            Actor *z0 = character_change_array[0].playerPointer, *z1 = character_change_array[1].playerPointer;
            f32 dz0 = SQ(z0->x_position - D_global_asm_807FDCB8) + SQ(z0->z_position - D_global_asm_807FDCBC);
            f32 dz1 = SQ(z1->x_position - D_global_asm_807FDCB8) + SQ(z1->z_position - D_global_asm_807FDCBC);
            return dz1 < dz0 ? z1 : z0;
        }
    }
    return gPlayerPointer;
}
// 1 while P2 enters a single-player map (door walk + cutscene) or comes back out of it (his walk), else -1
static int coop_enter_user(void) {
    if (coop_mg_pending || coop_mg_ret > 0 || coop_door_trig == 1) return 1;
    if (coop_isles_scene && coop_isles_who == 1 && current_map == coop_isles_map) return 1;
    if (coop_cs_trig == 1 && is_cutscene_active != 0) return 1;
    return -1;
}
// Called when a cutscene starts (playCutscene): in the Japes lobby the B. Locker cutscenes (idx not a world door) belong to the Kong closest to him
void coop_cs_pre(int idx, Actor *subject) {
    int i;
    Actor *lk = NULL;
    Actor *k0, *k1;
    f32 d0, d1;
    if (!coop_active_here() || character_change_array == NULL) return;
    k0 = character_change_array[0].playerPointer;
    k1 = character_change_array[1].playerPointer;
    if (!character_change_array[1].does_player_exist || k0 == NULL || k1 == NULL) return;
    if (current_map != MAP_JAPES_LOBBY || idx == 28 || idx == 29 || idx == 30) return;
    for (i = 0; i < D_global_asm_807FBB34; i++) {
        Actor *a = D_global_asm_807FB930[i].unk0;
        if (a != NULL && a->unk58 == ACTOR_B_LOCKER) { lk = a; break; }
    }
    if (lk == NULL) return;
    d0 = SQ(k0->x_position - lk->x_position) + SQ(k0->z_position - lk->z_position);
    d1 = SQ(k1->x_position - lk->x_position) + SQ(k1->z_position - lk->z_position);
    coop_cs_trig = (d1 < d0) ? 1 : 0;
}
// Klumsy arrival scene: player (0/1) who walked in and for whom the cutscene commands bound to "the player" (roll, actions...) run, else -1
int coop_scene_who(void) {
    if (coop_active_here() && current_map == MAP_KLUMSY) return coop_isles_who;
    return -1;
}
// Klumsy arrival scene, P2 walked in: a text command of the cutscene spoken by "the player" (command 0x18 / 0x39 without a spawner) is
// spoken by P2, so the speech bubble comes out of his head. Returns P2's actor, else NULL.
Actor *coop_scene_speaker(s32 cmd, s32 a1) {
    int c = cmd & 0xFF;
    if (character_change_array == NULL || character_change_array[1].playerPointer == NULL) return NULL;
    if ((c == 0x18 || c == 0x39) && ((a1 >> 16) & 0xFFFF) == 0 && coop_scene_who() == 1) {
        return character_change_array[1].playerPointer;
    }
    return NULL;
}
int coop_mg_colour(void) { return (coop_mg_swapped && !coop_map_allowed()) ? coop_mg_col : -1; }
static int coop_tg_saved = 0;
static int coop_tg_hold = 0;   // back from the lab / a barrel: the player who stayed is kept at his saved spot until the scene is over
static int coop_tg_hold_t = 0; // frames spent in Training Grounds since the return
static int coop_tg_fade = -1;  // player who entered the barrel / lab: his view shrinks to the split screen when he comes back (-1: none)
static f32 coop_tg_x, coop_tg_y, coop_tg_z;
static s16 coop_tg_rot;

// ---- Story co-op: choice of the two Kongs on the stats screen of the file menu (patches_ui.c draws the cursors) ----
// Each player moves his own cursor with the D-pad (left / right) under the Kongs that are unlocked; when P1 presses A the two positions are read and
// applied to the first co-op spawn of the game (func_global_asm_806C7D40). Both cursors start under Donkey Kong.
static u8 coop_menu_sel[2] = {0, 0};
static u8 coop_menu_pick[2] = {0, 0}; // the two choices read when P1 pressed A (the cursors themselves go back under Donkey Kong at once)
static int coop_menu_apply = 0;
void coop_menu_reset(void) { coop_menu_sel[0] = 0; coop_menu_sel[1] = 0; }
int coop_menu_get(int p) { return (p == 0 || p == 1) ? coop_menu_sel[p] : 0; }
void coop_menu_move(const s8 *unlocked) {
    int p;
    for (p = 0; p < 2; p++) {
        int dir = 0, j;
        if (unlocked[coop_menu_sel[p]] == 0) coop_menu_sel[p] = 0; // (another save with fewer Kongs unlocked)
        if (newly_pressed_input[p] & 0x0200) dir = -1; // D-pad left
        else if (newly_pressed_input[p] & 0x0100) dir = 1; // D-pad right
        if (dir == 0) continue;
        for (j = (int)coop_menu_sel[p] + dir; j >= 0 && j < 5; j += dir) {
            if (unlocked[j] != 0) {
                coop_menu_sel[p] = (u8)j;
                playSound(0x74, 0x7FFF, 63.0f, 1.0f, 0, 0); // same sound as when a Kong / colour is changed on the battle selection screen
                break;
            }
        }
    }
}
void coop_menu_commit(void) {
    coop_menu_pick[0] = coop_menu_sel[0]; coop_menu_pick[1] = coop_menu_sel[1];
    coop_menu_apply = 1;
    coop_menu_reset(); // the next visit of the stats screen starts again under Donkey Kong
}

// ---- Story co-op: what a Kong buys in a shop (Funky: weapon, ammo belt, scope, homing ammo; Candy: instrument, upgrades; Cranky: moves) is also
// given to the other Kong (for free). The shop is a single-player map played by the Kong who went in (persona in slot 0, the other one in slot 1: see
// coop_mg_swap_persona). At the shop's load the buyer's data is remembered; when the world is loaded again (persona not yet given back) what he gained
// is copied to the other Kong's own entry of the progress (per-Kong data of both Kongs live in player 0's block) and to the other player's ammo pools.
static int coop_sh_active = 0;
static int coop_sh_go = 0; // a shop is being entered from a co-op map with both players
static u8 coop_sh_kong = 0;
static u8 coop_sh_b[10];
static u16 coop_sh_std, coop_sh_hom;
static void coop_shop_snapshot(void) {
    int i;
    u8 k = current_character_index[0];
    if (k >= 6) return;
    coop_sh_kong = k;
    for (i = 0; i < 10; i++) coop_sh_b[i] = D_global_asm_807FC950[k * 0x5E + i];
    coop_sh_std = *(u16 *)&D_global_asm_807FC950[0x2F0];
    coop_sh_hom = *(u16 *)&D_global_asm_807FC950[0x2F0 + 2];
    coop_sh_active = 1;
}
static void coop_shop_mirror(void) {
    u8 k = coop_sh_kong;
    u8 o = current_character_index[1];
    u8 *kb, *ko;
    u16 *pstd, *phom, std_max, hom_max, ostd_max, ohom_max;
    coop_sh_active = 0;
    if (k >= 6 || o >= 6) return;
    kb = &D_global_asm_807FC950[k * 0x5E];
    ko = &D_global_asm_807FC950[o * 0x5E];
    // (both players can play the same Kong: the per-Kong fields are then shared and only the ammo pools below have to be copied)
    if (o != k) {
    if (kb[0] > coop_sh_b[0] && ko[0] < kb[0]) ko[0] = kb[0];       // moves (level)
    if (kb[1] > coop_sh_b[1] && ko[1] < kb[1]) ko[1] = kb[1];       // simian slam level
    ko[2] |= kb[2] & ~coop_sh_b[2];                                  // weapon bits (gun, homing, scope)
    if (kb[3] > coop_sh_b[3] && ko[3] < kb[3]) ko[3] = kb[3];       // ammo belt
    ko[4] |= kb[4] & ~coop_sh_b[4];                                  // instrument bits
    {
        // coins: what the buyer spent is taken from the other Kong too (the coins are shared between the Kongs)
        u16 cb = *(u16 *)&kb[6], cbb = (u16)((coop_sh_b[6] << 8) | coop_sh_b[7]), *co = (u16 *)&ko[6];
        if (cb < cbb) {
            u16 spent = (u16)(cbb - cb);
            *co = (*co > spent) ? (u16)(*co - spent) : 0;
        }
    }
    {
        u16 ba = *(u16 *)&kb[8], bb = (u16)((coop_sh_b[8] << 8) | coop_sh_b[9]), *oa = (u16 *)&ko[8];
        if (ba > bb && *oa < ba) *oa = ba;                           // instrument ammo (refilled / upgraded)
    }
    }
    // ammo pools: the buyer's pools are in block 0 (slot 0), the other player's in block 1
    pstd = (u16 *)&D_global_asm_807FC950[0x306 + 0x2F0];
    phom = (u16 *)&D_global_asm_807FC950[0x306 + 0x2F2];
    std_max = (u16)((1 << kb[3]) * 50);
    hom_max = (u16)(kb[3] * 5 + 20);
    ostd_max = (u16)((1 << ko[3]) * 50);
    ohom_max = (u16)(ko[3] * 5 + 20);
    {
        u16 now = *(u16 *)&D_global_asm_807FC950[0x2F0];
        if (now > coop_sh_std) {
            u32 v = (now >= std_max) ? ostd_max : (u32)*pstd + (now - coop_sh_std); // refilled to the top: the other one is too
            *pstd = (u16)(v > ostd_max ? ostd_max : v);
        }
    }
    {
        u16 now = *(u16 *)&D_global_asm_807FC950[0x2F2];
        if (now > coop_sh_hom) {
            u32 v = (now >= hom_max) ? ohom_max : (u32)*phom + (now - coop_sh_hom);
            *phom = (u16)(v > ohom_max ? ohom_max : v);
        }
    }
}

RECOMP_PATCH void func_global_asm_806C7D40(s32 arg0) {
    s32 var_v0;
    s32 i;
    CharacterChange *temp_s0;
    CoopVpEntry *temp_v0_2;
    s32 coop_n;
    s32 coop_mode;

    // Jalon 1 architecture: the game keeps cc_number_of_players == 1 (story rules everywhere) but we create
    // TWO Kong actors / two character_change entries.
    coop_n = (recomp_get_coop_enabled() && coop_map_allowed() && D_global_asm_80750AC0 >= 2) ? 2 : cc_number_of_players;
    coop_mode = (coop_n == 2 && cc_number_of_players == 1);
    if (current_map == MAP_MAIN_MENU) { coop_p2_kong_mem = 0xFF; coop_tg_free = 0; coop_tg_saved = 0; coop_tg_hold = 0; coop_tg_fade = -1; coop_isles_scene = 0; }
    if (coop_map_allowed() && current_map != coop_tg_map) { coop_tg_saved = 0; coop_tg_fade = -1; }
    // shop left: what the buyer got is given to the other Kong (before the persona goes back to its slot)
    if (coop_sh_active && coop_map_allowed() && !coop_is_shop(current_map)) coop_shop_mirror();
    // Bonus barrel (or lab) played by P2: his persona (Kong, ammo, health) takes the player 0 slot of the single-player map; given back on return
    if (coop_mg_swapped && coop_map_allowed()) {
        coop_mg_swap_persona();
        coop_mg_swapped = 0;
        coop_mg_ret = 240;
    } else if (coop_mg_pending) {
        if (!coop_map_allowed() && !coop_mg_swapped) { coop_mg_swap_persona(); coop_mg_swapped = 1; }
        coop_mg_pending = 0;
    }
    // shop entered (with two players): remember what the buyer owns now
    if (coop_is_shop(current_map) && coop_sh_go) { coop_shop_snapshot(); coop_sh_go = 0; }
    // back from a crown arena: J1 reappears at the arena warp, P2 would be at the world's start; he is put beside J1 (see func_global_asm_8067889C)
    coop_arena_back = (coop_arena_was && coop_map_allowed() && !coop_is_arena(current_map)) ? 40 : 0;
    coop_arena_was = coop_is_arena(current_map);
    coop_door_ret1 = (coop_door_p1 && coop_map_allowed()) ? 240 : 0; coop_door_p1 = 0;
    coop_door_p2 = 0; // (exits are always walked by P1: no P2 walker any more)
    coop_door_trig = -1; // the Kong who walked in is not needed any more: the arrival is walked by P1
    if (current_map == MAP_MAIN_MENU) coop_mg_swapped = 0; // (re)loading the game: the next co-op spawn starts again from P1's Kong + 1
    if (coop_mode) {
        // P2 starts with a copy of P1's progress (health, moves...)
        {
            // P2 keeps its own health between maps (only its first spawn / a change of max health takes P1's value)
            static u8 coop_p2_hp_valid = 0;
            s8 keep_hp = (s8)D_global_asm_807FC950[0x306 + 0x2FB];
            u8 keep_pool[10];
            for (i = 0; i < 10; i++) keep_pool[i] = D_global_asm_807FC950[0x306 + 0x2F0 + i]; // P2's own ammo/oranges/crystals/film
            u8 keep_mel = D_global_asm_807FC950[0x306 + 0x2FC];
            for (i = 0; i < 0x306; i++) {
                D_global_asm_807FC950[0x306 + i] = D_global_asm_807FC950[i];
            }
            if (coop_p2_hp_valid && keep_mel == D_global_asm_807FC950[0x2FC] && keep_hp > 0) {
                D_global_asm_807FC950[0x306 + 0x2FB] = (u8)keep_hp;
            }
            if (coop_p2_hp_valid) {
                for (i = 0; i < 10; i++) D_global_asm_807FC950[0x306 + 0x2F0 + i] = keep_pool[i];
            }
            D_global_asm_807FC950[0x306 + 0x2FD] = 0;
            coop_p2_hp_valid = 1;
        }
        // choice made on the file menu (stats screen): the first co-op spawn after launching the game uses the Kongs chosen by the two players
        if (coop_menu_apply && current_map != MAP_MAIN_MENU) {
            if (coop_kong_unlocked(coop_menu_pick[0])) current_character_index[0] = coop_menu_pick[0];
            if (coop_kong_unlocked(coop_menu_pick[1])) { current_character_index[1] = coop_menu_pick[1]; coop_p2_kong_mem = coop_menu_pick[1]; }
        }
        // P2 plays a different Kong than P1, except in a boss arena (the boss is made for one Kong: both players play it)
        if (current_map == MAP_JAPES_ARMY_DILLO) {
            current_character_index[1] = current_character_index[0];
        } else if (coop_p2_kong_mem <= 4 && coop_kong_unlocked(coop_p2_kong_mem)) {
            current_character_index[1] = coop_p2_kong_mem; // P2 keeps his own Kong across warps
        } else if (current_character_index[1] == current_character_index[0] || current_character_index[1] > 4 || !coop_kong_unlocked(current_character_index[1])) {
            // first spawn after loading the game: the next unlocked Kong after P1's; none -> P2 plays the same Kong as P1
            int k, pick = current_character_index[0];
            for (k = 1; k < 5; k++) {
                int c = (current_character_index[0] + k) % 5;
                if (coop_kong_unlocked(c)) { pick = c; break; }
            }
            current_character_index[1] = pick;
        }
    }
    if (current_map != MAP_MAIN_MENU) coop_menu_apply = 0; // (only the first map after the file menu uses the choice)
    character_change_array = _malloc(coop_n * 0x2F0);
    func_global_asm_80611690(character_change_array);
    for (i = 0; i < coop_n; i++) {
        temp_s0 = &character_change_array[i];
        temp_s0->does_player_exist = TRUE;
        cc_player_index = i;
        func_global_asm_806C850C(arg0, i);
        temp_s0->playerPointer = gCurrentPlayer;
        if (i == 0) {
            gPlayerPointer = gCurrentPlayer;
        }
        temp_s0->look_at_eye_z = 100.0f;
        temp_s0->look_at_eye_y = 100.0f;
        temp_s0->look_at_eye_x = 100.0f;
        temp_s0->unk224 = 100.0f;
        temp_s0->unk220 = 100.0f;
        temp_s0->unk21C = 100.0f;
        temp_s0->look_at_at_z = 0.0f;
        temp_s0->look_at_at_y = 0.0f;
        temp_s0->look_at_at_x = 0.0f;
        temp_s0->look_at_up_z = 0.0f;
        temp_s0->look_at_up_x = 0.0f;
        temp_s0->look_at_up_y = 1.0f;
        // viewport table lookup is keyed by player count: use the 2-player entries (split-screen) in co-op
        if (coop_mode) { cc_number_of_players = 2; }
        temp_v0_2 = func_global_asm_806C7C94(i);
        if (coop_mode) { cc_number_of_players = 1; }
        temp_s0->unk270[0] = temp_v0_2->unk4;
        temp_s0->unk270[1] = temp_v0_2->unk6;
        temp_s0->unk270[2] = temp_v0_2->unk8;
        temp_s0->unk270[3] = temp_v0_2->unkA;
        if (D_global_asm_8074450C == 2) { // original: osViGetCurrentMode() == hi-res mode; the port always runs 240p (=1)
            temp_s0->unk270[0] <<= 1;
            temp_s0->unk270[1] <<= 1;
            temp_s0->unk270[2]++;
            temp_s0->unk270[2] <<= 1;
            temp_s0->unk270[2]--;
            temp_s0->unk270[3]++;
            temp_s0->unk270[3] <<= 1;
            temp_s0->unk270[3]--;
        }
        temp_s0->unk278_arr[0] = temp_s0->unk270[2] - temp_s0->unk270[0];
        temp_s0->unk278_arr[1] = temp_s0->unk270[3] - temp_s0->unk270[1];
        temp_s0->fov_y = 45.0f;
        temp_s0->near = 10.0f;
        temp_s0->unk280 = (f32)temp_s0->unk278_arr[0] / temp_s0->unk278_arr[1];
        if (coop_mode && i < 2) {
            int k;
            for (k = 0; k < 4; k++) coop_vp_rect[i][k] = temp_s0->unk270[k];
            coop_vp_asp[i] = temp_s0->unk280;
            coop_vp_saved = 1;
        }
        temp_s0->far = D_global_asm_807444C4;
        temp_s0->chunk = 0;
        temp_s0->unk294 = &D_global_asm_807ECD10[i];
        temp_s0->new_controller_inputs = (OSContPad *)&newly_pressed_input[i];
        temp_s0->action_initiated = -1;
        temp_s0->unk2C0 = 1;
        temp_s0->unk2C1 = 1;
        temp_s0->unk2EA = 0;
        temp_s0->unk2EB = 0;
        func_global_asm_80708BB8(&temp_s0->unk2DC);
        if (
            ((D_global_asm_807FBB64 & 1) && (!(D_global_asm_807FBB68 & 2))) ||
            (COOP_PP_HEALTH(i) + COOP_PP_2FD(i)) <= 0
        ) {
            func_global_asm_80709464(i);
        }
    }
    if (D_global_asm_807FD570 != 0) {
        func_global_asm_806C80E4();
        D_global_asm_807FD570 = 0;
    }
    if (cc_number_of_players >= 2) {
        spawnActor(ACTOR_UNKNOWN_318, 0);
    }
    var_v0 = D_global_asm_80750AB8 == 0;
    if (var_v0 != 0) {
        var_v0 = ((s32) cc_number_of_players < 2) ^ 1;
    }
    D_global_asm_80750AD0 = var_v0;
}

// ---- Story co-op: invulnerability frames after a hit ----
// Originals only set the post-hit invulnerability timer (unkD4) when cc_number_of_players < 2
// (Battle uses another mechanism). In co-op we want the story behaviour: otherwise the hit and
// its sound repeat every frame while touching an enemy.
extern Actor *gCurrentActorPointer;
extern PlayerAdditionalActorData *extra_player_info_pointer;
extern void playAnimation(Actor *, s32);
extern void playSoundAtActorPosition(Actor *, s16, u8, s16, u8);
extern u8 D_global_asm_807530B0[];
extern u8 D_global_asm_807FD584;
extern s32 func_global_asm_806EB400(Actor *);
extern void func_global_asm_806CA2AC(void);
extern s32 func_global_asm_806C9974(u8, s8);
extern void func_global_asm_8061F0B0(void *, s32, s32);

static int coop_story_rules(void) {
    return cc_number_of_players < 2 || (recomp_get_coop_enabled() && coop_map_allowed());
}

RECOMP_PATCH void func_global_asm_806EE2B8(void) {
    s32 sp24;
    s8 phi_a1;

    sp24 = TRUE;
    if (func_global_asm_806EB400(gCurrentPlayer)) {
        phi_a1 = -1;
        func_global_asm_806CA2AC();
        if (character_change_array[cc_player_index].unk2A0 && ((Actor *)character_change_array[cc_player_index].unk2A0)->unk58 == ACTOR_KOSHA) {
            phi_a1 = -2;
        }
        if (func_global_asm_806C9974(cc_player_index, phi_a1)) {
            sp24 = FALSE;
            if (gCurrentPlayer->unk6A & 4) {
                playAnimation(gCurrentPlayer, 0x29);
            } else {
                playAnimation(gCurrentPlayer, 0x27);
            }
            gCurrentPlayer->control_state = 0x36;
            gCurrentPlayer->control_state_progress = 0;
        } else {
            playSoundAtActorPosition(gCurrentActorPointer, 0x16, 0xFF, 0x7F, 0);
            playAnimation(gCurrentActorPointer, 0x1E);
            gCurrentActorPointer->control_state = 0x20;
            gCurrentActorPointer->control_state_progress = 1;
        }
    }
    if ((extra_player_info_pointer->unkD4 == 0) && coop_story_rules()) {
        extra_player_info_pointer->unkD4 = D_global_asm_807530B0[D_global_asm_807FD584];
    }
    if (sp24) {
        func_global_asm_8061F0B0(extra_player_info_pointer->unk104, 0x14, 0x14);
    }
}

RECOMP_PATCH void func_global_asm_806EE454(void) {
    s32 sp24;

    sp24 = TRUE;
    func_global_asm_806CA2AC();
    if (func_global_asm_806EB400(gCurrentPlayer) && func_global_asm_806C9974(cc_player_index, -1)) {
        sp24 = FALSE;
        if ((gCurrentPlayer->unk6A & 4) != 0) {
            playAnimation(gCurrentPlayer, 0x29);
        } else {
            playAnimation(gCurrentPlayer, 0x27);
        }
        gCurrentPlayer->control_state = 0x36;
        gCurrentPlayer->control_state_progress = 0;
    } else {
        playSoundAtActorPosition(gCurrentActorPointer, 0x16, 0xFF, 0x7F, 0);
        playAnimation(gCurrentActorPointer, 0x1E);
        gCurrentActorPointer->control_state = 0x20;
        gCurrentActorPointer->control_state_progress = 1;
    }
    if ((extra_player_info_pointer->unkD4 == 0) && coop_story_rules()) {
        extra_player_info_pointer->unkD4 = D_global_asm_807530B0[D_global_asm_807FD584];
    }
    if (sp24) {
        func_global_asm_8061F0B0(extra_player_info_pointer->unk104, 0x14, 0x14);
    }
}

// ---- Story co-op: central damage function (identical to the original) ----
extern void func_global_asm_8060E7EC(u8, u8, u8);
extern void func_multiplayer_80026E20(u8, s8); // overlay (Kong Battle)
extern s32 D_global_asm_807552EC;

static u8 coop_melon_owner = 0;   // Kong that touched the last queued melon slice
static u8 coop_melon_pending = 0; // a queued slice heal is waiting to be applied
static u8 coop_pickup_player; // forward declaration (defined below): owner of the pickup being processed
RECOMP_PATCH s32 func_global_asm_806C9974(u8 playerIndex, s8 arg1) {
    CharacterChange *temp2;

    // Co-op: a melon slice (+1) picked up by P2 is applied by the deferred pickup queue as "player 0"; heal P2 instead.
    if (recomp_get_coop_enabled() && coop_map_allowed() && arg1 > 0 && coop_melon_pending) {
        // heal goes to the Kong that touched the slice (recorded when the pickup was queued)
        playerIndex = coop_melon_owner;
        coop_melon_pending = 0;
    }
    temp2 = &character_change_array[playerIndex];

    temp2->unk2DC.unk6 |= 0x11;
    // Kong Battle only (overlay function): skipped in co-op story maps where the overlay isn't loaded
    if ((cc_number_of_players >= 2) && (arg1 < 0) && (D_global_asm_807552EC == 1) && !(recomp_get_coop_enabled() && coop_map_allowed())) {
        func_multiplayer_80026E20(playerIndex, arg1);
    }
    if (arg1 < 0) {
        func_global_asm_8060E7EC(playerIndex, 0xFF, 5);
    }
    if (!(D_global_asm_807FBB64 & 0x800)) {
        D_global_asm_807FC950[playerIndex * 0x306 + 0x2FD] += arg1;
    }
    if ((COOP_PP_HEALTH(playerIndex) + COOP_PP_2FD(playerIndex)) <= 0) {
        temp2->unk2DC.unk6 |= 0xC0;
        return TRUE;
    } else {
        return FALSE;
    }
}

// ---- Story co-op: Battle-only branches removed on co-op maps ----
static int coop_active_here(void) {
    return recomp_get_coop_enabled() && coop_map_allowed();
}
int coop_colour_active(void) { return coop_active_here(); }
// Story co-op colours: decided when a player changes Kong, then kept as long as he keeps that Kong.
// Same Kong as the other player -> the colour index the other one does not use; otherwise index 0.
static int coop_col_idx[2] = {0, 0};
static int coop_col_kong[2] = {-1, -1};
static int coop_col_stamp = -1;
int coop_colour_index(int player) {
    int p;
    if (player < 0 || player > 1 || !coop_active_here()) return 0;
    if ((int)object_timer != coop_col_stamp) {
        coop_col_stamp = (int)object_timer;
        for (p = 0; p < 2; p++) {
            Actor *me = character_change_array[p].playerPointer;
            Actor *ot = character_change_array[1 - p].playerPointer;
            if (!character_change_array[p].does_player_exist || me == NULL) continue;
            if (coop_col_kong[p] != (int)me->unk58) {
                coop_col_kong[p] = (int)me->unk58;
                if (ot != NULL && character_change_array[1 - p].does_player_exist && coop_col_kong[1 - p] != -1 && (int)ot->unk58 == (int)me->unk58) {
                    coop_col_idx[p] = (coop_col_idx[1 - p] == 1) ? 0 : 1;
                } else {
                    coop_col_idx[p] = 0;
                }
            }
        }
    }
    return coop_col_idx[player];
}

// Player count / viewport setup. Same as the original, plus: in co-op, drop the "multiplayer map" flag
// right after the reset check in 805FE7FC (it was only needed to keep 2 players), so the rest of the load
// (map objects, camera mode, Battle-only overlay calls gated by this flag) behaves like the story.
RECOMP_PATCH void func_global_asm_806C7C10(void) {
    cc_number_of_players = D_global_asm_80750AC0;
    character_change_array = NULL;
    if (cc_number_of_players >= 2) {
        switch (D_global_asm_80750AB8) {
            case 0:
                D_global_asm_80750AB4 = cc_number_of_players;
                break;
            case 1:
                D_global_asm_80750AB4 = 1;
                break;
        }
        if (coop_active_here()) {
            D_global_asm_807FBB64 &= ~0x04000000;
            // Load the map (objects, setup, spawners...) under single-player rules: the player count is restored
            // to 2 at the start of func_global_asm_806C7D40, just before the players are created.
            cc_number_of_players = 1;
        }
    } else {
        D_global_asm_80750AB4 = 1;
        D_global_asm_80750AB8 = 0;
    }
}

// Melon segments: 4 in story, 10 in multiplayer
RECOMP_PATCH s32 func_global_asm_8070943C(void) {
    return (cc_number_of_players > 1 && !coop_active_here()) ? 10 : 4;
}

extern void func_multiplayer_80026B0C(s32); // overlay (Kong Battle)
extern void func_multiplayer_80026D40(Actor *, s32); // overlay (Kong Battle)
extern s32 D_global_asm_807FBB68;
extern s32 gameIsInAdventureMode(void);
extern s32 gameIsInMysteryMenuMinigameMode(void);
extern s32 func_global_asm_805FCA64(void);
extern u32 global_properties_bitfield;

// Health update from pending damage (unk2FD). Same as the original; the Battle-only overlay call on death is skipped in co-op.
RECOMP_PATCH void func_global_asm_80708C24(s32 playerIndex, CharacterChange *arg1, u8 *pp, s32 arg3) {
    s32 segmentsPerMelon;
    u8 sp28;
    s8 *health = (s8 *)&pp[0x2FB];
    u8 melons = pp[0x2FC];
    s8 *pending = (s8 *)&pp[0x2FD];

    sp28 = arg1->unk2DC.unk6;
    if (sp28 & 0x10) {
        arg1->unk2DC.unk6 ^= 0x10;
        if (*pending > 0) {
            segmentsPerMelon = func_global_asm_8070943C();
            s32 v = *health + *pending;
            s32 m = melons * segmentsPerMelon;
            *health = v < m ? v : m;
        } else if (*pending < 0) {
            if (gameIsInAdventureMode() || gameIsInMysteryMenuMinigameMode()) {
                *health += *pending;
            }
            if (*health <= 0) {
                *health = 0;
                arg1->unk2DC.unkA = 0xC;
                if (cc_number_of_players >= 2 && !coop_active_here()) {
                    func_multiplayer_80026B0C(playerIndex);
                }
            } else {
                playSound(0x24B, 0x7FFF, 63.0f, 1.0f, 0, 0);
            }
        }
        *pending = 0;
    }
    if ((!(global_properties_bitfield & 2) && func_global_asm_805FCA64()) || (sp28 & 0x80)) {
        arg1->unk2DC.unk9--;
        if (arg1->unk2DC.unk9 == 0) {
            arg1->unk2DC.unk9 = 0x11;
            arg1->unk2DC.unk8 = 3;
        }
    }
}

// Death / game-over handler. Same as the original, but in co-op the Battle respawn (overlay) is replaced by the story path.
extern void func_global_asm_80709464(s32);
extern void func_global_asm_805FF378(s32, s32);
extern void func_global_asm_805FF800(s32 *, s32 *);
extern void func_global_asm_805FF1B0(s32);
extern void setFlag(s16, u8, u8);

RECOMP_PATCH void func_global_asm_80712830(Actor *arg0, s32 exit) {
    s32 map;
    s32 sp20;
    s32 sp1C;

    func_global_asm_80709464(exit);
    if (coop_active_here()) {
        func_global_asm_80709464(0);
        func_global_asm_80709464(1);
    }
    if (D_global_asm_807FBB64 & 1) {
        arg0->control_state_progress++;
        if (D_global_asm_807FBB68 & 2) {
            // K. Rool: Reset Tiny Phase Progress Flags and Round Counter
            setFlag(TEMPFLAG_RESET_TINY_PHASE, TRUE, FLAG_TYPE_TEMPORARY);
            map = MAP_DK_ISLES_OVERWORLD;
            exit = 0xC;
        } else {
            map = MAP_TROFF_N_SCOFF;
            if (current_map == MAP_FUNGI_SPIDER) {
                map = MAP_FUNGI_REAR_PART_OF_MILL;
                exit = 1;
            } else {
                exit = 2;
            }
        }
        func_global_asm_805FF378(map, exit);
        return;
    }
    if (D_global_asm_807FBB64 & 0x400) {
        arg0->control_state_progress++;
        func_global_asm_805FF800(&sp20, &sp1C);
        func_global_asm_805FF378(sp20, sp1C);
        return;
    }
    if (cc_number_of_players >= 2 && !coop_active_here()) {
        func_multiplayer_80026D40(arg0, exit);
    } else {
        func_global_asm_805FF1B0(exit);
    }
}


// ---- Story co-op: death camera ----
// The death scene (cutscene camera) is always built for player 0's camera and centred on gPlayerPointer. In co-op it is
// centred on the Kong that is dying, and both viewports show it (see func_global_asm_8062BF24).
extern u8 func_global_asm_80671E00(f32, f32, f32, f32, s16 *, s16 *, u8, u16);
extern s32 playCutscene(Actor *, s16, u8);
RECOMP_PATCH void func_global_asm_80627948(Actor *arg0, u8 arg1, u8 arg2, u8 arg3) {
    s16 sp2E;
    s16 sp2C;

    if (coop_active_here() && arg0 == gPlayerPointer && character_change_array[cc_player_index].playerPointer != NULL) {
        arg0 = character_change_array[cc_player_index].playerPointer;
    }
    if (func_global_asm_80671E00(arg0->x_position, arg0->y_position, arg0->z_position, 80.0f, &sp2E, &sp2C, 1, 0xBC)) {
        arg0->y_rotation = func_global_asm_80665DE0(character_change_array->look_at_eye_x, character_change_array->look_at_eye_z, arg0->x_position, arg0->z_position);
        playCutscene(arg0, arg2, arg3);
    } else {
        playCutscene(arg0, arg1, arg3);
    }
}

extern s16 D_global_asm_807476F4;
extern u16 D_global_asm_807F5CF4;
extern Actor *D_global_asm_807F5CE8;
extern Actor *D_global_asm_807F5D10;
static int coop_cannon_cam_map = -1; // map in which that camera actor was saved (stale after a map change)
static Actor *coop_cannon_cam_saved = NULL; // P1's camera actor while the cutscene is bound to P2's camera
static int coop_barrel_busy = 0;              // P2 is choosing in the tag barrel: the scene (camera 0) is shown on P2's screen only
static Actor *coop_barrel_user_actor = NULL; // the Kong who entered the barrel
static int coop_barrel_chooser = -1;          // player index (0/1) choosing in the tag barrel, -1 if none (refreshed every frame by the barrel)
static u32 coop_barrel_stamp = 0;             // object_timer of the last barrel update with a selection running
static Actor *coop_barrel_spot = NULL;        // spotlight actor of the selection scene
static int coop_barrel_sel = 0;               // a tag barrel selection is running (P1 or P2): no "glue P2 to P1" during it
// Japes cannon (control states 0x42 entering / 0x45 flying): index of the Kong using a cannon, -1 if none
static int coop_cannon_user(void) {
    int k;
    if (!coop_active_here() || current_map == MAP_TRAINING_GROUNDS) return -1; // (state 0x42 is also the bonus barrel entry)
    for (k = 0; k < 2; k++) {
        Actor *p = character_change_array[k].playerPointer;
        if (character_change_array[k].does_player_exist && p != NULL && (p->control_state == 0x42 || p->control_state == 0x45)) return k;
    }
    return -1;
}
// Training Grounds: player (0/1) who is entering a bonus barrel (state 62 / 66), else -1
static int coop_barrel_user(void) {
    int k;
    if (!coop_active_here() || current_map != MAP_TRAINING_GROUNDS || character_change_array == NULL) return -1;
    for (k = 1; k >= 0; k--) {
        Actor *p = character_change_array[k].playerPointer;
        if (character_change_array[k].does_player_exist && p != NULL && (p->control_state == 62 || p->control_state == 66)) return k;
    }
    return -1;
}
// Japes cannon cutscene: returns 1 for the camera actor of the Kong who is NOT in the cannon (it must keep its normal
// follow mode instead of running the cutscene script, see func_global_asm_8061DBD4 in patches_interpolation.c).
int coop_cannon_other_cam(Actor *cam) {
    int u;
    Actor *owner;
    static int pre_frames;
    if (is_cutscene_active != 1 || current_map != MAP_JAPES || D_global_asm_807476F4 != 22) { pre_frames = 0; return 0; }
    u = coop_cannon_user();
    if (u < 0) {
        // the first frame(s) of the cutscene: the Kong in the cannon is not known yet -> neither camera runs the cutscene
        // (no one-frame flash of the cutscene view on the wrong screen), at most 4 calls per camera so that nothing can hang
        if (pre_frames < 8) { pre_frames++; return 1; }
        return 0;
    }
    pre_frames = 0;
    owner = character_change_array[u].playerPointer;
    if (owner == NULL || owner->PaaD == NULL) return 0;
    return cam != ((PlayerAdditionalActorData *)owner->PaaD)->unk104;
}
static int coop_death_cam(void) {
    int k;
    if (!coop_active_here() || is_cutscene_active == 0) return 0;
    for (k = 0; k < 2; k++) {
        Actor *p = character_change_array[k].playerPointer;
        if (character_change_array[k].does_player_exist && p != NULL && (p->control_state == 0x36 || p->control_state == 0x39)) return 1;
    }
    return 0;
}

static int coop_barrel_chooser_now(void) {
    if (!coop_active_here() || coop_barrel_chooser < 0) return -1;
    if ((u32)(object_timer - coop_barrel_stamp) > 3) return -1; // barrel not updating any more (map change...)
    return coop_barrel_chooser;
}

// Player whose viewport is drawn on the WHOLE screen (the other one is not drawn), or -1 for the normal split screen.
// - tag barrel selection: the player who is choosing;
// - scripted cutscenes (speech bubbles of Scoff/Troff, Squawks...): both Kongs see the same scene, so only one view is shown.
//   Not for the Japes cannon (one Kong flies while the other keeps playing) nor the death camera.
#define COOP_P_FULLSCREEN_CS 1
extern u8 D_global_asm_807FC8B9;
extern u8 is_autowalking;
// World door entry / exit: the walk is shown full screen, then the split screen comes back while the cutscene (Kong shrinking /
// growing and spinning) is still running. 0 = keep the full screen until the end of that cutscene (old behaviour).
#define COOP_ENTRY_EARLY_SPLIT 1
#define COOP_ENTRY_SPLIT_AT 56 // tick (since the map start) at which the split screen comes back after a world door cutscene
#define COOP_ENTRY_DELAY 16 // ticks of full screen kept after the walk of a world door (10 for caves / lobbies)
static int coop_ex_player = -1;   // entry transition running: player whose view is growing to full screen (both views are still drawn)
static int coop_arrive_pending = 0; // a map change between co-op maps was requested (func_global_asm_805FF378): the next map is an arrival
static int coop_fs_walk = 0; // 1 when the last coop_fullscreen_raw() full screen only came from a door / cave walk (no cutscene or scene)
static int coop_fullscreen_raw(void) {
    static u32 fs_stamp = 0;
    static u32 aw_stamp = 0;
    static int aw_on = 0;
    static int fs_player = -1;
    static int fs_feed = 0;
    static int fs_entry = 0;
    static int aw_player = 0; // player whose view is shown full screen during the walk (the one who walks)
    static int entry_cs = 0; // current cutscene started with the map (world door entry / exit)
    static int fs_map = -1;
    static int aw_prev = 0;
    static int arr_ph = 0, arr_seen = 0;
    static int aw_arrival = 0; // the current walk is an arrival walk (starts right after the map load): no full screen, both views are shown
    static int hs_fed = 0; // Troff 'n' Scoff: a Kong has fed the hippos in this visit (the first visit's introduction cutscene is over)
    int ch = coop_barrel_chooser_now();
    coop_fs_walk = 0;
    // the entry full screen ends with the map change: the arrival is shown in split screen
    if (fs_map != (int)current_map) { fs_map = (int)current_map; aw_on = 0; hs_fed = 0; arr_ph = coop_arrive_pending; coop_arrive_pending = 0; arr_seen = 0; }
    // arrival phase (co-op map -> co-op map): from the map start to the end of the exit walk and of the door cutscene; the view of the
    // Kong who walks is NOT shown full screen during it
    if (arr_ph) {
        if (is_autowalking == 3) arr_seen = 1;
        else if (arr_seen && (is_cutscene_active != 1 || (u32)object_timer > 120)) arr_ph = 0;
        else if ((u32)object_timer > 600) arr_ph = 0;
    }
    if (is_autowalking == 3 && !aw_prev) aw_arrival = ((u32)object_timer < 40 || (current_map == MAP_TRAINING_GROUNDS && coop_tg_cave));
    aw_prev = (is_autowalking == 3);
    // cutscene that starts with the map (world door entry / exit); must be tracked while the entry walk (early return below) runs
    if (is_cutscene_active != 1) entry_cs = 0;
    else if ((u32)object_timer < 12 && (current_map != MAP_TRAINING_GROUNDS || coop_tg_cave)) entry_cs = 1; // (Training Grounds: new game intro and the scene after Cranky's lab are real cutscenes, not door entries; the cave from DK Isles is one)
    if (ch >= 0) return ch;
    // Troff 'n' Scoff, first visit: the pig's explanation and the camera shot on the banana feeding pad are played on P1's screen, full screen, until
    // the end of the cutscene (then the split screen comes back); the feeding itself (state 0x6C) keeps the rules below
    if (coop_active_here() && current_map == MAP_TROFF_N_SCOFF) {
        Actor *h0 = character_change_array[0].playerPointer;
        Actor *h1 = character_change_array[1].playerPointer;
        if ((h0 != NULL && h0->control_state == 0x6C) || (h1 != NULL && h1->control_state == 0x6C)) hs_fed = 1;
        if (!hs_fed && is_cutscene_active == 1 && is_autowalking != 3 && !coop_death_cam()) return 0;
    }
    // Jungle Japes arrival cutscene (parrot, Hott&Stroff door, parrot; cutscene 18): played on P1's screen only, full screen, whoever walked in
    // (the script state is global: with the split screen both cameras advanced it, it ran at double speed)
    // and while the parrot's last lines are still shown after the cutscene (the pads are locked, see func_global_asm_8067889C)
    if (coop_active_here() && current_map == MAP_JAPES && ((is_cutscene_active == 1 && D_global_asm_807476F4 == 18)
        || (coop_jp18 && (int)object_timer >= coop_txt_last && (int)object_timer - coop_txt_last <= 3))) return 0;
    // first exit of the Training Grounds (parrot + cutscene towards the next place): the Kong who walked out keeps the whole screen
    // (Training Grounds -> DK Isles: only the first time has a scene (parrot); the other times it is a plain walk, shown like any arrival,
    // so the full screen only starts once a real cutscene has been seen)
    if (coop_isles_scene && coop_active_here() && (coop_isles_trip || coop_isles_cs) && (current_map == coop_isles_map || (coop_isles_trip && current_map == coop_isles_from))) return coop_isles_who;
    // pause menu: the split screen stays as it is (the menu is drawn over it, for the player who pressed START)
    // door / cave / lobby entry and exit walk (is_autowalking == 3): the whole screen shows P1's view, the split screen comes back
    // a few frames after the walk is over
    if (coop_active_here() && is_autowalking == 3 && !aw_arrival && !arr_ph) {
        aw_on = 1; aw_stamp = (u32)object_timer;
        aw_player = (D_global_asm_807FD710 != NULL && D_global_asm_807FD710 == character_change_array[1].playerPointer) ? 1 : 0;
        coop_fs_walk = 1;
        return aw_player;
    }
    if (aw_on) {
        coop_fs_walk = 1;
        if (COOP_ENTRY_EARLY_SPLIT && entry_cs && (u32)object_timer <= COOP_ENTRY_SPLIT_AT && coop_active_here()) return (aw_player == 1 && coop_enter_user() == 1) ? 1 : 0; // world door entry / exit: same split moment for both
        if ((u32)((u32)object_timer - aw_stamp) <= (u32)((COOP_ENTRY_EARLY_SPLIT && entry_cs) ? COOP_ENTRY_DELAY : 10) && coop_active_here()) return (aw_player == 1 && coop_enter_user() == 1) ? 1 : 0;
        coop_fs_walk = 0;
        aw_on = 0;
    }
#if COOP_P_FULLSCREEN_CS
    if (coop_active_here() && !coop_death_cam() && coop_cannon_user() < 0 && !(arr_ph && coop_cs_trig < 0 && ch < 0)
        && !(current_map == MAP_JAPES && D_global_asm_807476F4 == 22)) {
        Actor *c0 = character_change_array[0].playerPointer;
        Actor *c1 = character_change_array[1].playerPointer;
        int want = -1;
        // door entry / exit cutscene: the full screen only lasts for the walk (is_autowalking == 3), then the split screen comes
        // back while the cutscene goes on
        if (is_cutscene_active == 1 && !(COOP_ENTRY_EARLY_SPLIT && entry_cs && (is_autowalking != 3 || aw_arrival))) {
            want = (c1 != NULL && D_global_asm_807F5CE8 == c1) ? 1 : 0;
            if (coop_door_trig >= 0 && !entry_cs) want = coop_door_trig;
        } else if (c0 != NULL && c0->control_state == 0x6C) {
            want = 0; // a Kong feeding Troff/Scoff (state 0x6C): the cutscene flag drops to 0 between the camera shots
        } else if (c1 != NULL && c1->control_state == 0x6C) {
            want = 1;
        }
        if (is_cutscene_active == 1 && coop_barrel_user() >= 0) want = coop_barrel_user();
        if (is_cutscene_active == 1 && current_map == MAP_TRAINING_GROUNDS && coop_barrel_user() < 0 && coop_enter_user() == 1) want = 1;
        if (is_cutscene_active == 1 && coop_cs_trig >= 0) want = coop_cs_trig; // Japes lobby, B. Locker: the Kong who triggered it has the screen // back from Cranky's lab entered by P2: his view, full screen
        if (want >= 0) {
            fs_stamp = (u32)object_timer;
            fs_player = want;
            fs_feed = (is_cutscene_active != 1); // kept because of the feeding state, not a cutscene
            if ((u32)object_timer < 12 && current_map != MAP_TRAINING_GROUNDS) fs_entry = 1; // cutscene that starts with the map (world door entry / exit): no long hold afterwards
            return want;
        }
        // Hold after the scene: short between two camera shots; after the feeding of Troff/Scoff the hippo's animation and the
        // camera shot on it go on for a few seconds (controls still locked), so hold longer unless a player touches a control.
        if (fs_player >= 0) {
            u32 age = (u32)((u32)object_timer - fs_stamp);
            u32 maxage = fs_feed ? 300 : (fs_entry ? 2 : 20);
            if (fs_feed && age >= 30) {
                int k;
                for (k = 0; k < 2; k++) {
                    if (D_global_asm_807ECD10[k].button != 0 || D_global_asm_807ECD10[k].stick_x > 30 || D_global_asm_807ECD10[k].stick_x < -30
                        || D_global_asm_807ECD10[k].stick_y > 30 || D_global_asm_807ECD10[k].stick_y < -30) { maxage = 0; }
                }
            }
            if (age <= maxage) return fs_player;
        }
    }
#endif
    fs_player = -1;
    fs_entry = 0;
    return -1;
}

static int coop_fullscreen_now(void) {
    if (coop_ex_player >= 0) return -1;
    return coop_fullscreen_raw();
}

// Troff 'n' Scoff feeding: the hippo script (806BE09C) asks for its camera effects on the camera actor taken from the player data,
// and in co-op that was P2's camera (log: cam idx 1) instead of the feeding Kong's: P1 (shown full screen) never saw the
// effect. Remap the effect to the camera of the Kong that is the target of the first effect, and keep it for the following
// ones (target = the hippo).
Actor *coop_fx_cam(Actor *cam, Actor *target) {
    static Actor *owner_cam = NULL;
    static u32 stamp = 0;
    Actor *kc[2] = {NULL, NULL};
    int k;
    if (!coop_active_here() || current_map != MAP_TROFF_N_SCOFF || cam == NULL) return cam;
    for (k = 0; k < 2; k++) {
        Actor *pa = character_change_array[k].playerPointer;
        kc[k] = (pa != NULL && pa->PaaD != NULL) ? ((PlayerAdditionalActorData *)pa->PaaD)->unk104 : NULL;
    }
    // the Kong that is feeding (state 0x6C) owns the effect
    for (k = 0; k < 2; k++) {
        Actor *pa = character_change_array[k].playerPointer;
        if (pa != NULL && pa->control_state == 0x6C && kc[k] != NULL && (target == pa || target == gPlayerPointer)) {
            owner_cam = kc[k];
            stamp = (u32)object_timer;
            return kc[k];
        }
    }
    for (k = 0; k < 2; k++) {
        if (target != NULL && target == character_change_array[k].playerPointer && kc[k] != NULL && target->control_state == 0x6C) {
            owner_cam = kc[k];
            stamp = (u32)object_timer;
            return kc[k];
        }
    }
    if ((cam == kc[0] || cam == kc[1]) && owner_cam != NULL && (u32)((u32)object_timer - stamp) < 900) return owner_cam;
    return cam;
}

// ---- Split-screen transition ----
// When a full-screen scene ends (entry/exit walk, cutscene, hippo, tag barrel), the split screen does not pop in: the view that was
// shown full screen shrinks to its half while the other player's view appears. Not used when leaving the pause menu.
#define COOP_TR_TICKS 6      // split screen coming back
#define COOP_TR_TICKS_IN 9   // entry: the view grows to full screen
static int coop_tr_player = -1;   // player whose view is the big one (-1: no transition running)
static int coop_tr_mode = 0;      // 0: split screen coming back (view shrinks), 1: entry (view grows to full screen)
static u32 coop_tr_start = 0;
static int coop_tr_map = 0;
static int coop_tr_prev = -1;
static int coop_tr_prev_pause = 0;
static int coop_tr_scene = 0;     // the full screen being left included a cutscene / scene (not only an entry / exit walk)
static int coop_tr_split = 120;   // current y of the line between the two views
// y of the line between the two views, "age" ticks after the start of the running transition
static int coop_tr_split_at(u32 age) {
    int fin = coop_vp_rect[0][3], big = (coop_tr_player == 0) ? 240 : 0;
    int from = coop_tr_mode ? fin : big, to = coop_tr_mode ? big : fin;
    u32 dur = coop_tr_mode ? COOP_TR_TICKS_IN : COOP_TR_TICKS;
    f32 t = (f32)age / (f32)dur, e;
    int r;
    if (t > 1.0f) t = 1.0f;
    e = t * t * (3.0f - 2.0f * t);
    r = from + (int)((f32)(to - from) * e);
    if (r < 4) r = 4;
    if (r > 236) r = 236;
    return r;
}
extern u8 D_global_asm_80770DC9;
static void coop_tr_update(void) {
    int fs, pause_;
    if (!coop_active_here()) { if (current_map == MAP_MAIN_MENU) { coop_tr_prev = -1; coop_tr_player = -1; coop_ex_player = -1; } return; }
    fs = coop_fullscreen_raw();
    pause_ = (global_properties_bitfield & 2) ? 1 : 0;
    // back from a barrel (no full-screen scene on return): the view of the Kong who came out shrinks to the split screen
    if (coop_tg_fade >= 0 && current_map == coop_tg_map && (u32)object_timer < 90) { // (only right after the map load)
        if (fs < 0 && !pause_) { coop_tr_prev = coop_tg_fade; coop_tr_prev_pause = 0; }
        coop_tg_fade = -1;
        coop_tr_scene = 0; // plain return from a barrel / the lab: both views are already there
    }
    {
        // barrel entry: the view grows to full screen on the first frame of the barrel state, even if the full screen was already on
        static int bu_prev = -1;
        int bu = (current_map == MAP_TRAINING_GROUNDS) ? coop_barrel_user() : -1;
        if (bu >= 0 && bu_prev < 0 && fs >= 0 && coop_ex_player < 0 && coop_vp_saved) coop_tr_prev = -1;
        bu_prev = bu;
    }
    if (fs >= 0 && !coop_fs_walk) coop_tr_scene = 1;
    if (fs >= 0) {
        if (coop_ex_player < 0) {
            Actor *c0_ = character_change_array[0].playerPointer;
            // entry into a cave / lobby / world door (walk or shrinking cutscene): the view grows to full screen instead of popping
            if (coop_tr_prev < 0 && coop_vp_saved && (is_autowalking == 3 || (c0_ != NULL && (c0_->control_state == 135 || (current_map == MAP_TRAINING_GROUNDS && coop_barrel_user() >= 0))))) {
                coop_ex_player = fs;
                coop_tr_player = fs;
                coop_tr_mode = 1;
                coop_tr_start = (u32)object_timer;
                coop_tr_map = (int)current_map;
            } else {
                coop_tr_player = -1;
            }
        }
    } else {
        coop_ex_player = -1;
        // the split screen only shrinks back after a cutscene / scene; after a plain arrival (walk, barrel, lab) both views just appear
        if (coop_tr_prev >= 0 && !coop_tr_prev_pause && coop_vp_saved && coop_tr_scene) {
            coop_tr_player = coop_tr_prev;
            coop_tr_mode = 0;
            coop_tr_start = (u32)object_timer;
        }
        coop_tr_scene = 0;
    }
    if (coop_tr_player >= 0) {
        u32 age;
        f32 t, e;
        int fin = coop_vp_rect[0][3], big = (coop_tr_player == 0) ? 240 : 0;
        int from = coop_tr_mode ? fin : big, to = coop_tr_mode ? big : fin;
        if ((u32)object_timer < coop_tr_start) coop_tr_start = (u32)object_timer;
        age = (u32)object_timer - coop_tr_start;
        u32 dur = coop_tr_mode ? COOP_TR_TICKS_IN : COOP_TR_TICKS;
        if (age >= dur || (coop_tr_mode && coop_tr_map != (int)current_map)) {
            coop_tr_player = -1;
            coop_ex_player = -1;
        } else {
            t = (f32)age / (f32)dur;
            e = t * t * (3.0f - 2.0f * t);
            coop_tr_split = from + (int)((f32)(to - from) * e);
            if (coop_tr_split < 4) coop_tr_split = 4;
            if (coop_tr_split > 236) coop_tr_split = 236;
        }
    }
    coop_tr_prev = fs;
    coop_tr_prev_pause = pause_;
}
int coop_transition_query(void) { return coop_tr_player >= 0; }

// Warp pad swipe (screen capture drawn over the scene, types 2 and 6 of func_global_asm_806291B4): it is limited to the half of the
// Kong who warps, the other half keeps showing the live scene.
static int coop_fx_pl = -1;
void coop_fx_begin(int type) {
    int k;
    coop_fx_pl = -1;
    if ((type != 2 && type != 6) || !coop_active_here() || !coop_vp_saved || !character_change_array[1].does_player_exist) return;
    for (k = 0; k < 2; k++) {
        if (gCurrentActorPointer != NULL && gCurrentActorPointer == character_change_array[k].playerPointer) coop_fx_pl = k;
    }
}
// Storm rain (K. Rool's ship...): the weather script spawns the particles of each Kong separately; a particle is drawn only in the
// half of the Kong it belongs to (owner 0/1; -1 while spawning for a single player = whole screen as in the original).
static int coop_wx_own = -1;
void coop_wx_set_owner(int p) { coop_wx_own = p; }
int coop_wx_get_owner(void) { return coop_wx_own; }
int coop_wx_two(void) {
    return coop_active_here() && coop_vp_saved && character_change_array[1].does_player_exist && character_change_array[1].playerPointer != NULL;
}
// 0: do not draw the particles of this owner; 1: draw them, clipped to the rows [top, bottom]
int coop_wx_range(int owner, int *top, int *bottom) {
    int fs;
    *top = 0; *bottom = 240;
    if (!coop_wx_two()) return 1;
    fs = coop_fullscreen_now();
    if (fs >= 0) return fs == owner;
    *top = coop_vp_rect[owner][1];
    *bottom = coop_vp_rect[owner][3];
    return 1;
}
// 1 and the y range (top, bottom) of the warper's half, else 0 (whole screen)
int coop_fx_range(int *top, int *bottom) {
    if (coop_fx_pl < 0 || !coop_active_here() || !coop_vp_saved || coop_fullscreen_now() >= 0) return 0;
    *top = coop_vp_rect[coop_fx_pl][1];
    *bottom = coop_vp_rect[coop_fx_pl][3];
    return 1;
}

int coop_fullscreen_query(void) { return coop_fullscreen_now(); }
// Player (0/1) whose Kong is in the Troff 'n' Scoff feeding state (0x6C) and is the one shown full screen, else -1
int coop_feeder_query(void) {
    int fs = coop_fullscreen_now();
    Actor *a;
    if (fs < 0 || fs > 1) return -1;
    a = character_change_array[fs].playerPointer;
    return (a != NULL && a->control_state == 0x6C) ? fs : -1;
}

// Camera update (8061DBD4): 1 when camera ci must NOT run the cutscene script because the other Kong's view is the one shown full screen.
int coop_cs_other_cam(int ci) {
    int fs;
    if (!coop_active_here() || is_cutscene_active != 1) return 0;
    fs = coop_fullscreen_now();
    return (fs >= 0 && fs != ci) ? 1 : 0;
}

extern s16 func_global_asm_80665DE0(f32, f32, f32, f32);
extern void renderActor(Actor *arg0, u8 arg1);
// Called by the actor draw (func_global_asm_80630DCC). Hook kept for the tag barrel scene (rotation is handled in the
// line-up update, func_global_asm_80683158). Returns 1 to skip the actor (never for now).
int coop_barrel_scene_actor(Actor *a) {
    int ch = coop_barrel_chooser_now();
    if (ch < 0) return 0;
    if (!((a->object_properties_bitfield & 0x40000000) || a == coop_barrel_spot)) return 0;
    return 0;
}

// Tag barrel line-up Kongs (update): original faces character_change_array[0]'s camera; in co-op they face the camera of
// the player who is choosing.
typedef struct { u16 unk0; u16 unk2; void *unk4; s32 unk8; } CoopAAD_80683158;
extern void func_global_asm_806F1C04(void);
extern void func_global_asm_806F22A8(void);
extern void func_global_asm_806F1EB0(void);
extern void func_global_asm_806C8220(s32, void *, s32);
extern s32 func_global_asm_8072881C(s32, s32 *);
RECOMP_PATCH void func_global_asm_80683158(void) {
    s32 sp3C;
    CoopAAD_80683158 *aaD;
    s32 (*fn)(Actor *, AnimationStateUnk1C *, s32 *);
    f32 ex, ez;
    int ch;

    sp3C = 0;
    aaD = gCurrentActorPointer->additional_actor_data;
    if (ACTOR_UNINITIALIZED(gCurrentActorPointer)) {
        gCurrentActorPointer->object_properties_bitfield |= 0x1000;
        gCurrentActorPointer->animation_state->unk1C = _malloc(32 * sizeof(AnimationStateUnk1C));
        func_global_asm_80611690(gCurrentActorPointer->animation_state->unk1C);
        func_global_asm_806C8220(0, (void *)gCurrentActorPointer->unk178, aaD->unk0);
        switch (aaD->unk0) {
            case 2: aaD->unk4 = func_global_asm_806F1C04; break;
            case 3: aaD->unk4 = func_global_asm_806F22A8; break;
            case 5: aaD->unk4 = func_global_asm_806F1EB0; break;
        }
    } else {
        fn = aaD->unk4;
        if (fn != NULL) {
            fn(gCurrentActorPointer, gCurrentActorPointer->animation_state->unk1C, &sp3C);
        }
    }
    gCurrentActorPointer->animation_state->unk1C[sp3C].unk0 = 0;
    gCurrentActorPointer->unk16D = gCurrentActorPointer->unk15F;
    ex = character_change_array->look_at_eye_x;
    ez = character_change_array->look_at_eye_z;
    ch = coop_barrel_chooser_now();
    if (ch == 1) {
        ex = character_change_array[1].unk21C;
        ez = character_change_array[1].unk224;
    }
    gCurrentActorPointer->y_rotation = func_global_asm_80665DE0(ex, ez, gCurrentActorPointer->x_position, gCurrentActorPointer->z_position);
    if ((func_global_asm_8072881C(0, &aaD->unk8) != 0) && ((aaD->unk0 == 3) || (aaD->unk0 == 5))) {
        func_global_asm_8072881C(0x81, &aaD->unk8);
    }
    renderActor(gCurrentActorPointer, 0);
}

// ---- Story co-op: actors (enemies...) load/unload around BOTH Kongs ----
// The spawner distance test only looked at the first cc_number_of_players cameras (1 in co-op). It now uses the closest of P1/P2.
RECOMP_PATCH f32 func_global_asm_80689DD4(f32 x, f32 y, f32 z) {
    f32 closest = 100000000.0f;
    f32 d;
    s32 i;
    s32 n = coop_active_here() ? 2 : cc_number_of_players;
    for (i = 0; i < n; i++) {
        if (character_change_array[i].does_player_exist) {
            d = (x - character_change_array[i].look_at_eye_x) * (x - character_change_array[i].look_at_eye_x)
              + (y - character_change_array[i].look_at_eye_y) * (y - character_change_array[i].look_at_eye_y)
              + (z - character_change_array[i].look_at_eye_z) * (z - character_change_array[i].look_at_eye_z);
            if (d < closest) closest = d;
            if (n > 1) {
                // co-op: P2's look_at_eye is not reliably updated; the camera position used for rendering (unk21C..) is
                d = (x - character_change_array[i].unk21C) * (x - character_change_array[i].unk21C)
                  + (y - character_change_array[i].unk220) * (y - character_change_array[i].unk220)
                  + (z - character_change_array[i].unk224) * (z - character_change_array[i].unk224);
                if (d < closest) closest = d;
            }
        }
    }
    if (n > 1) {
        static int sdb = 0, scalls = 0;
        if (sdb < 40 && ((++scalls) % 300) == 0) {
            sdb++;
        }
    }
    return closest;
}

// Nearest living Kong to an actor (used so that enemies target the closest player instead of always P1).
// Kong who last picked up a golden banana, and when: the map-object scripts (prop controller actor) follow him while the banana's scenes run
static u8 coop_cc_saved = 0; // cc_player_index while the prop controller runs
static int coop_cc_swapped = 0;
static int coop_gb_picker = -1;
static u32 coop_gb_stamp = 0;
#define COOP_GB_SCRIPT_TICKS 450 // (the Diddy / coconut switches scene of Japes starts ~150 ticks after the dance)
// Golden banana dance: the Kong who did not pick it up is brought next to the picker by the cutscene glue (see the end of func_global_asm_8067889C);
// his place is remembered at the pickup and he is put back there (same heading) once the dance scene is over.
static int coop_gbtp_on = 0;
static int coop_gbtp_who = 0;
static u32 coop_gbtp_t = 0;
static f32 coop_gbtp_x, coop_gbtp_y, coop_gbtp_z;
static s16 coop_gbtp_ang;
static int coop_gbtp_quiet = 0; // frames without cutscene since the pickup (the Diddy scene follows the dance within a few frames)
static int coop_gbd_on = 0;     // a golden banana dance scene is running: pair centred on the pickup place
static int coop_gbd_who = 0;    // the picker
static f32 coop_gbd_x, coop_gbd_y, coop_gbd_z;
static int coop_gbd_have = 0;   // the side vector below is fixed (it does not follow the heading any more once the dance has started)
static f32 coop_gbd_sx, coop_gbd_sz;
#define COOP_GBD_HALF 12.5f     // half the distance between the two dancers (flip the sign here if the pair ends up swapped / off-centre)
static Actor *coop_nearest_kong(Actor *a) {
    Actor *best = NULL;
    f32 bestd = 0.0f;
    int k;
    // The map-object scripts (the "prop controller" actor, which also starts their cutscenes) are written for player 0: they read gPlayerPointer.
    // With the nearest Kong they saw the wrong one (e.g. the Diddy / coconut switches scene of Japes was never started when P1 picked the banana
    // and P2, glued beside him, was the nearest). They follow P1, except for a while after P2 picked up a golden banana.
    if (a->unk58 == ACTOR_PROP_CONTROLLER) {
        Actor *p2_ = character_change_array[1].playerPointer;
        if (coop_gb_picker == 1 && (u32)((u32)object_timer - coop_gb_stamp) < COOP_GB_SCRIPT_TICKS && character_change_array[1].does_player_exist && p2_ != NULL) return p2_;
        return character_change_array[0].playerPointer;
    }
    for (k = 0; k < 2; k++) {
        Actor *p = character_change_array[k].playerPointer;
        f32 dx, dy, dz, d;
        if (!character_change_array[k].does_player_exist || p == NULL) continue;
        dx = p->x_position - a->x_position;
        dy = p->y_position - a->y_position;
        dz = p->z_position - a->z_position;
        d = dx * dx + dy * dy + dz * dz;
        if (p->control_state == 0x36 || p->control_state == 0x39) d += 1.0e12f; // dying: prefer the other one
        if (best == NULL || d < bestd) { best = p; bestd = d; }
    }
    return best;
}

// ---- Story co-op: "current player" context for non-player actors ----
// The original only switches the per-player context (gCurrentPlayer, cc_player_index, ...) when the
// actor being updated is a player. After P2 is updated, that context stays on P2 for every later
// actor (enemies...), so they only ever see P2. In co-op, non-player actors get P1's context back.
extern void func_global_asm_806663F8(void);
extern void func_global_asm_8067B100(Actor *);
extern void func_global_asm_80678F64(Actor *);
extern void func_global_asm_80675EE0(Actor *);
extern void func_global_asm_806761EC(Actor *);
extern void func_global_asm_806794EC(Actor *);
extern void func_global_asm_806EB194(void);
extern void func_global_asm_80672B1C(Actor *);
extern void func_global_asm_80669A6C(Actor *);
extern u8 D_global_asm_807F94B4;

RECOMP_PATCH void func_global_asm_80678BBC(Actor *arg0) {
    func_global_asm_806663F8();
    if (coop_active_here() && (arg0->interactable & 1)) {
        int pi = ((PlayerAdditionalActorData *)arg0->PaaD)->unk1A4 & 1;
        {
            // In single-player the death state (0x36) never completes on its own for a non-multiplayer game; in co-op
            // switch to the "fade out + reload" state (0x39) after the death animation so the level restarts for both Kongs.
            static u16 coop_death_t[2] = {0, 0};
            if (arg0->control_state == 0x36) {
                if (++coop_death_t[pi] >= 90) {
                    arg0->control_state = 0x39;
                    arg0->control_state_progress = 1;
                    ((PlayerAdditionalActorData *)arg0->PaaD)->unk11C = 20;
                    coop_death_t[pi] = 0;
                }
            } else {
                coop_death_t[pi] = 0;
            }
        }
    }
    if (arg0->interactable & 1) {
        func_global_asm_8067B100(arg0);
    } else if (coop_active_here() && gPlayerPointer != NULL) {
        func_global_asm_8067B100(gPlayerPointer);
    }
    arg0->unk88 = arg0->x_position;
    arg0->unk8C = arg0->y_position;
    arg0->unk90 = arg0->z_position;
    arg0->unkBC = arg0->unkB8;
    D_global_asm_807F94B4 = 0;
    if (arg0->ledge_info_pointer) {
        arg0->ledge_info_pointer->unk68 = 0;
    }
    func_global_asm_80678F64(arg0);
    if (arg0 == gCurrentPlayer) {
        func_global_asm_80675EE0(arg0);
    } else {
        func_global_asm_806761EC(arg0);
    }
    func_global_asm_806794EC(arg0);
    if (arg0 == gCurrentPlayer) {
        func_global_asm_806EB194();
    }
    if (arg0->noclip_byte & 4) {
        func_global_asm_80672B1C(arg0);
    } else {
        if (arg0->ledge_info_pointer) {
            arg0->ledge_info_pointer->unk6C = 0;
        }
    }
    if (arg0->locked_to_pad) {
        func_global_asm_80669A6C(arg0);
    }
}

// ---- Story co-op: camera-facing sprites (melon slices, bananas...) for P2's viewport ----
// A world sprite's model matrix (sprite->unk128[0]) is built once per frame so that the sprite faces PLAYER 0's camera
// (rotation R0 such that R0 * V0 is a pure scale). Drawn in P2's viewport it is seen edge-on. For P2 a corrected matrix
// R1 = R0 * V0 * V1^-1 is built (V = rotation part of each camera's view matrix), so the sprite faces P2's camera too.
// Only done when R0 * V0 really is a billboard (diagonal); other sprites are left alone.
extern void guMtxF2L(float mf[4][4], Mtx *m);
static f32 coop_mtx_f(void *mtx, int k) {
    u32 *w = (u32 *)mtx;
    u32 wi = w[k >> 1], wf = w[8 + (k >> 1)];
    s16 ip = (s16)((k & 1) ? (wi & 0xFFFF) : (wi >> 16));
    u16 fp = (u16)((k & 1) ? (wf & 0xFFFF) : (wf >> 16));
    return (f32)ip + (f32)fp / 65536.0f;
}
static u32 coop_sprite_buf[1024][16] __attribute__((aligned(16)));
static u32 coop_sprite_cnt = 0;
extern u8 D_global_asm_807444FC;
// Slot of sprite->unk128[] to use for the viewport being drawn. The original update (80715270) builds one billboard matrix per
// player i < AB4 from character_change_array[i]'s render eye, but the draw only indexes by cc_player_index when
// cc_number_of_players > 1 (it is 1 in co-op), so P2 used P1's matrix.
extern u8 D_global_asm_80750AB4;
int coop_sprite_slot(void) {
    if (cc_number_of_players > 1) return cc_player_index;
    if (cc_player_index == 1 && coop_active_here() && D_global_asm_80750AB4 >= 2
        && character_change_array != NULL && character_change_array[1].does_player_exist) return 1;
    return 0;
}
void *coop_sprite_mtx(void *m0, int raw_alignment, void *vtx) {
    if (coop_sprite_slot() == 1) return m0; // the matrix already faces P2's camera (built in the update from its own eye)
    f32 R0[3][3], V0[3][3], V1[3][3], P[3][3], T[3][3], R1[3][3], out[4][4];
    f32 n1, d = 0.0f;
    int r, c, k;
    if (cc_player_index != 1 || !coop_active_here() || !character_change_array[0].does_player_exist || !character_change_array[1].does_player_exist) {
        return m0;
    }
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) {
            R0[r][c] = coop_mtx_f(m0, r * 4 + c);
            V0[r][c] = coop_mtx_f(&character_change_array[0].unk8[D_global_asm_807444FC], r * 4 + c);
            V1[r][c] = coop_mtx_f(&character_change_array[1].unk8[D_global_asm_807444FC], r * 4 + c);
        }
    }
    for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) {
        P[r][c] = 0.0f;
        for (k = 0; k < 3; k++) P[r][c] += R0[r][k] * V0[k][c];
    }
    if (raw_alignment != 0) return m0;
    // World sprites are flat quads: x/y axes (rows 0 and 1 of R0) built from camera 0's right/up vectors, row 2 is zero.
    for (c = 0; c < 3; c++) {
        if (R0[2][c] > 0.01f || R0[2][c] < -0.01f) return m0;
    }
    (void)d;
    n1 = V1[0][0] * V1[0][0] + V1[0][1] * V1[0][1] + V1[0][2] * V1[0][2];
    if (n1 < 0.0001f) return m0;
    for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) {
        T[r][c] = 0.0f;
        for (k = 0; k < 3; k++) T[r][c] += P[r][k] * V1[c][k]; // P * V1^T
        T[r][c] /= n1;
    }
    // R1 = R0 * V0 * V1^-1 would give a matrix that looks identical on screen; we want R1 * V1 = R0 * V0, so R1 = P * V1^-1
    for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) R1[r][c] = T[r][c];
    for (r = 0; r < 4; r++) for (c = 0; c < 4; c++) out[r][c] = 0.0f;
    for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) out[r][c] = R1[r][c];
    for (c = 0; c < 3; c++) out[3][c] = coop_mtx_f(m0, 12 + c);
    out[3][3] = 1.0f;
    {
        u32 idx = coop_sprite_cnt++ & 1023;
        guMtxF2L(out, (Mtx *)coop_sprite_buf[idx]);
        return coop_sprite_buf[idx];
    }
}

// ---- Story co-op: map rendering (func_global_asm_8062C29C) ----
// Identical to the original except the last argument of func_global_asm_8062D620 (the "split-screen" flag,
// AB4 >= 2), which is 0 in co-op so the map is drawn at full detail (sky, etc.) in each viewport.
extern s32 D_global_asm_807F5DE4;
extern s32 D_global_asm_807F5E60;
extern s32 D_global_asm_807F5FF0;
extern u8 D_global_asm_807F5FEC;
extern void *D_global_asm_807F5DE8;
extern void *D_global_asm_807F5DEC;
extern f32 D_global_asm_807F5E68;
extern s32 D_global_asm_807F5E64;
extern f32 D_global_asm_807F5FA8;
extern f32 D_global_asm_807F5FAC;
extern f32 D_global_asm_807F5FB0;
extern f32 D_global_asm_807F5FB4;
extern f32 D_global_asm_807F5FDC;
extern f32 D_global_asm_807F5FE0;
extern u8 D_global_asm_807444FC;
extern void func_global_asm_8062DB70(f32, f32, f32, f32, f32, f32);
extern void func_global_asm_8062A944(f32, f32, f32);
extern void func_global_asm_8062AC68(void *);
extern void func_global_asm_8062D620(s32, s32, s32, f32, f32, f32, s32, s32, s32);
extern Gfx *func_global_asm_80722294(Gfx *, Actor *, s16);
extern Gfx *func_global_asm_8065919C(Gfx *);
extern Gfx *func_global_asm_8062CA70(Gfx *, s32, s32, f32, f32, f32, s32);
extern f32 func_global_asm_80612D10(f32);

RECOMP_PATCH Gfx *func_global_asm_8062C29C(Gfx *dl, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8, f32 arg9) {
    s32 pad[25];
    Gfx *temp_s3;
    temp_s3 = (Gfx *)&character_change_array[cc_player_index].unk1F0[D_global_asm_807444FC];
    D_global_asm_807F5E64 = 0;
    D_global_asm_807F5FB4 = arg7;
    D_global_asm_807F5FB0 = arg8;
    D_global_asm_807F5FAC = arg9;
    D_global_asm_807F5FA8 = D_global_asm_807F5FAC;
    D_global_asm_807F5FE0 = func_global_asm_80612D10(arg7 * 0.017453292f);
    D_global_asm_807F5FE0 = (((D_global_asm_807F5FE0 - D_global_asm_807F5FDC) * 3.0) + D_global_asm_807F5FDC);
    func_global_asm_8062DB70(arg1, arg2, arg3, arg4, arg5, arg6);
    func_global_asm_8062A944(D_global_asm_807F5FB4, D_global_asm_807F5FB0, D_global_asm_807F5FA8);
    func_global_asm_8062AC68(&character_change_array[cc_player_index].unk8[D_global_asm_807444FC]);
    func_global_asm_8062AD28(arg1, arg2, arg3, &D_global_asm_807F5E68, (f32 *)D_global_asm_807F5E20);
    func_global_asm_8062D620(D_global_asm_807F5DE4, D_global_asm_807F5E60, D_global_asm_807F5FF0, arg1, arg2, arg3, 0, !D_global_asm_807F5FEC, (D_global_asm_80750AB4 >= 2));
    gSPSegment(dl++, 0x06, osVirtualToPhysical(D_global_asm_807F5DE8));
    gSPSegment(dl++, 0x07, osVirtualToPhysical(D_global_asm_807F5DEC));
    dl = func_global_asm_80722294(dl, character_change_array[cc_player_index].playerPointer, cc_player_index);
    gSPSegment(dl++, 0x05, osVirtualToPhysical(temp_s3));
    gDPSetHilite1Tile(temp_s3++, G_TX_RENDERTILE, &character_change_array[cc_player_index].unk1D0[D_global_asm_807444FC], 32, 32);
    gSPEndDisplayList(temp_s3++);
    func_global_asm_8062C99C(&character_change_array[cc_player_index].unk250[D_global_asm_807444FC], character_change_array[cc_player_index].unk270[0], character_change_array[cc_player_index].unk270[1], character_change_array[cc_player_index].unk270[2], character_change_array[cc_player_index].unk270[3]);
    gSPPerspNormalize(dl++, character_change_array[cc_player_index].unk188);
    gSPViewport(dl++, osVirtualToPhysical(&character_change_array[cc_player_index].unk250[D_global_asm_807444FC]));
    gDPSetScissorFrac(dl++, G_SC_NON_INTERLACE, character_change_array[cc_player_index].unk270[0] * 4.0f, character_change_array[cc_player_index].unk270[1] * 4.0f, character_change_array[cc_player_index].unk270[2] * 4.0f, character_change_array[cc_player_index].unk270[3] * 4.0f);
    gSPLookAt(dl++, osVirtualToPhysical(&character_change_array[cc_player_index].unk190[D_global_asm_807444FC]));
    func_global_asm_80658E58(character_change_array[cc_player_index].unk270[0], character_change_array[cc_player_index].unk270[1], character_change_array[cc_player_index].unk270[2], character_change_array[cc_player_index].unk270[3]);
    dl = func_global_asm_8065919C(dl);
    dl = func_global_asm_8070835C(dl, cc_player_index);
    gSPMatrix(dl++, osVirtualToPhysical(&character_change_array[cc_player_index].unk88[D_global_asm_807444FC]), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(dl++, osVirtualToPhysical(&character_change_array[cc_player_index].unk8[D_global_asm_807444FC]), G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
    gSPMatrix(dl++, &D_2000180, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    dl = func_global_asm_8062CA70(dl, D_global_asm_807F5DE4, D_global_asm_807F5E60, arg1, arg2, arg3, global_properties_bitfield);
    gDPPipeSync(dl++);
    return dl;
}

// ---- Story co-op: actor update loop (target = closest Kong for enemies) ----
extern void (*D_global_asm_8074C0A0[])();
extern u16 D_global_asm_807FBB3C;
extern u8 D_global_asm_807FBE08;
extern void func_global_asm_80678CC8(Actor *);
extern void func_global_asm_80678BBC(Actor *);

extern u8 is_autowalking;
extern f32 D_global_asm_80753218[];
extern f32 D_global_asm_807531FC[];
#define COOP_P_ENEMY 1 // enemies/other actors target the closest Kong
extern GlobalASMStruct35 D_global_asm_807FBB70;
static int coop_tgt_swapped = 0;
// Player (0/1) the pad / trigger events of this frame were computed for (see func_global_asm_8062BAE4)
static int coop_pad_owner = 0;
extern u8 is_autowalking;
extern u8 D_global_asm_807FC8B9;
extern void func_global_asm_806A8158(void);
extern void func_global_asm_806A5868(void);
extern OSContPad D_global_asm_807ECD58;
extern u16 newly_pressed_input_copy;
extern u8 D_global_asm_807ECD08;
extern volatile u8 D_global_asm_807467C4;
// Raw pads as read from the controllers. The game zeroes the pads of the ports while the player is locked (cutscenes), but the
// speech bubbles still read player 0's raw pad (copied before that), so P2 needs his raw pad too.
static u16 coop_raw_btn[4];
static u16 coop_raw_new[4];
RECOMP_PATCH void func_global_asm_8060AC34(OSContPad *arg0) {
    if (D_global_asm_807467C4 == 0) {
        if (D_global_asm_807ECD08 != 0) {
            int i;
            osContGetReadData(arg0);
            for (i = 0; i < 4; i++) {
                coop_raw_new[i] = (u16)((coop_raw_btn[i] ^ arg0[i].button) & arg0[i].button);
                coop_raw_btn[i] = arg0[i].button;
            }
        }
    }
    D_global_asm_807ECD08 = 0;
}
static int coop_dbg_prev[2];
static int coop_pad_swap = 0; // 1 while func_global_asm_8062BAE4 runs the map scripts on behalf of P2
int coop_pad_swap_now(void) { return coop_pad_swap; }
// Gun switches (peanut / coconut... switches that open a door): the map scripts read P1's data (gPlayerPointer, Kong...).
// coop_gun_owner = player (0/1) who owns the latest live gun projectile, coop_gun_tick = when it was seen,
// coop_cs_noglue = the running cutscene started right after a shot (door scene): P2 is not teleported to P1.
extern s16 D_global_asm_807F6240[];
extern s32 func_global_asm_80659470(s32 arg0);
extern Prop *D_global_asm_807F6000;
static int coop_gun_owner = -1;
static u32 coop_gun_clock = 0; // own frame counter (object_timer is not monotonic)
static u32 coop_gun_tick = 0;
static int coop_cs_noglue = 0;
static int coop_cs_prev_active = 0;
// called by playCutscene: a cutscene that starts right after a shot of P2 (switch door) keeps P2 as script owner for a while
extern s16 D_global_asm_807FDCB8; // zone position (x) used by the loading zone walk
extern s16 D_global_asm_807FDCBC; // (z)
// called when a cutscene starts: world door scene (30 = going in): the Kong that is running (state 13) while the other stands walked in
void coop_door_pre(int idx, Actor *door) {
    if ((idx == 30 || idx == 28) && coop_active_here() && character_change_array != NULL && character_change_array[1].does_player_exist
        && character_change_array[0].playerPointer != NULL && character_change_array[1].playerPointer != NULL) {
        Actor *k0 = character_change_array[0].playerPointer, *k1 = character_change_array[1].playerPointer;
        if (door != NULL && door != k0 && door != k1) {
            // main criterion: the Kong that is the closest to the door actor is the one who walked into it (log: 157 vs 193926, 302 vs 297098)
            f32 d0 = SQ(k0->x_position - door->x_position) + SQ(k0->z_position - door->z_position);
            f32 d1 = SQ(k1->x_position - door->x_position) + SQ(k1->z_position - door->z_position);
            coop_door_trig = (d1 < d0) ? 1 : 0;
            coop_door_trig_t = coop_gun_clock;
        } else if (coop_dbg_st[1] == 13 && coop_dbg_st[0] != 13) { coop_door_trig = 1; coop_door_trig_t = coop_gun_clock; }
        else if (coop_dbg_st[0] == 13 && coop_dbg_st[1] != 13) { coop_door_trig = 0; coop_door_trig_t = coop_gun_clock; }
    }
}
int coop_cs_notify(int idx) {
    if (coop_active_here() && idx != 21 && coop_gun_owner == 1 && coop_gun_tick != 0 && coop_gun_clock + 1 - coop_gun_tick < 240) {
        return 1; // the cutscene (subject NULL = P1) is played for the Kong who shot
    }
    return 0;
}
#define COOP_GUN_START 240
RECOMP_PATCH void func_global_asm_8067889C(u8 arg0) {
    // Co-op: the autowalk sets the Kong's max walking speed (unk38) to the fast autowalk value and only the Kong
    // that goes through a state change gets it reset. A Kong spawned/triggered during an autowalk (P2) kept running
    // at that speed afterwards until it jumped/punched. Reset it once the autowalk is over.
    // Bonus barrel played by P2 (single-player map): the game reads controller 1 for the player, so the two controllers are swapped
    if (arg0 == 1 && coop_mg_swapped && !coop_map_allowed()) {
        OSContPad tp = D_global_asm_807ECD10[0];
        u16 tn = newly_pressed_input[0];
        if ((D_global_asm_807ECDF8 & 3) != (D_global_asm_807ECD09 & 3)) func_global_asm_8060AA58(3);
        D_global_asm_807ECD10[0] = D_global_asm_807ECD10[1];
        D_global_asm_807ECD10[1] = tp;
        newly_pressed_input[0] = newly_pressed_input[1];
        newly_pressed_input[1] = tn;
    }
    if (arg0 == 1 && coop_active_here() && character_change_array != NULL) {
        Actor *gc0 = character_change_array[0].playerPointer;
        coop_gun_clock++;
        {
            int ti;
            for (ti = 0; ti < 2; ti++) {
                Actor *tk = character_change_array[ti].playerPointer;
                if (tk != NULL) {
                    int ts = (int)tk->control_state;
                    if (ts != coop_dbg_st[ti]) { coop_dbg_prev[ti] = coop_dbg_st[ti]; coop_dbg_st[ti] = ts; }
                }
            }
            // world door swirl (state 135 for both): the Kong that walked in was in state 13 just before, the other one in state 12
            if (coop_dbg_st[0] == 135 && coop_dbg_st[1] == 135 && (coop_dbg_prev[0] == 13) != (coop_dbg_prev[1] == 13) && coop_door_trig < 0) { coop_door_trig = (coop_dbg_prev[1] == 13) ? 1 : 0; coop_door_trig_t = coop_gun_clock; }
            else if (coop_dbg_st[0] != 135 && coop_dbg_st[1] != 135 && coop_door_trig >= 0 && (u32)(coop_gun_clock - coop_door_trig_t) > 120) coop_door_trig = -1;
        }
        Actor *gc1 = character_change_array[1].playerPointer;
        int gi;
        for (gi = 0; gi < D_global_asm_807FBB34; gi++) {
            Actor *ga = D_global_asm_807FB930[gi].unk0;
            if (ga != NULL && (ga->unk58 == ACTOR_PROJECTILE_PEANUT || ga->unk58 == ACTOR_PROJECTILE_COCONUTS || ga->unk58 == ACTOR_PROJECTILE_GRAPE
                || ga->unk58 == ACTOR_PROJECTILE_PINEAPPLE || ga->unk58 == ACTOR_PROJECTILE_FEATHER)) {
                if (ga->unk11C != NULL && ga->unk11C == gc1) { coop_gun_owner = 1; coop_gun_tick = coop_gun_clock + 1; }
                else if (ga->unk11C != NULL && ga->unk11C == gc0) { coop_gun_owner = 0; coop_gun_tick = coop_gun_clock + 1; }
            }
        }
        if (coop_cs_noglue && is_cutscene_active != 0) coop_gun_tick = coop_gun_clock + 1; // door scene: window counts from its end
        if (is_cutscene_active == 0) {
            coop_cs_prev_active = 0;
            coop_cs_noglue = 0;
        } else if (!coop_cs_prev_active) {
            coop_cs_prev_active = 1;
            coop_cs_noglue = (coop_gun_tick != 0 && coop_gun_clock + 1 - coop_gun_tick < COOP_GUN_START) ? 1 : 0;
        }
    } else if (arg0 == 1 && !coop_active_here()) {
        coop_cs_noglue = 0; coop_cs_prev_active = 0; coop_gun_tick = 0;
    }
    if (arg0 == 1 && coop_active_here() && character_change_array != NULL && is_autowalking != 3) {
        int pi;
        for (pi = 0; pi < 2; pi++) {
            Actor *pa = character_change_array[pi].playerPointer;
            int kong = current_character_index[pi];
            if (pa != NULL && pa->PaaD != NULL && kong >= 0 && kong <= 4) {
                PlayerAdditionalActorData *pp = (PlayerAdditionalActorData *)pa->PaaD;
                if (pp->unk8C == 0 && !(pp->unk1F0 & 0x20) && pp->unk38 == D_global_asm_80753218[kong] && D_global_asm_80753218[kong] != D_global_asm_807531FC[kong]) {
                    pp->unk38 = D_global_asm_807531FC[kong];
                }
            }
        }
    }
    // Training Grounds: once the intro is over (60 frames without cutscene), P2 is never glued to P1 (Cranky's lab, barrels)
    static int tg_calm = 0;
    if (arg0 == 1) {
        if (current_map != MAP_TRAINING_GROUNDS) { tg_calm = 0; }
        else if (is_cutscene_active != 0) tg_calm = 0;
        else if (tg_calm < 60) tg_calm++;
        if (tg_calm >= 60) coop_tg_free = 1;
    }
    // DK Isles arrival scene: ends when no cutscene nor walk has run for a while (the fullscreen then fades back to the split screen)
    if (arg0 == 1 && coop_isles_scene) {
        if (coop_active_here() && current_map == coop_isles_map) {
            coop_isles_t++;
            // busy: cutscene, forced walk or cinematic bars (the game's own "controls locked" test); Klumsy's sequence has long pauses between scenes
            if (is_cutscene_active != 0 || is_autowalking == 3 || !func_global_asm_805FCA64()) { coop_isles_seen = 1; coop_isles_idle = 0; }
            else coop_isles_idle++;
            if (is_cutscene_active != 0) coop_isles_cs = 1;
            if (coop_isles_idle > ((coop_isles_trip && coop_isles_cs && !coop_isles_ret) ? 240 : 45) || coop_isles_t > 7200) coop_isles_scene = 0;
        } else if (current_map != coop_isles_from) {
            coop_isles_scene = 0;
        }
    }
    // back from Cranky's lab / a barrel: no teleport of the player who stayed (the cutscene scripts move player 0 to the exit)
    if (arg0 == 1 && coop_tg_hold && coop_active_here() && current_map == coop_tg_map && character_change_array != NULL) {
        Actor *hk = character_change_array[coop_tg_stayer].playerPointer;
        int busy = (is_cutscene_active != 0 || is_autowalking == 3);
        int moved = (D_global_asm_807ECD10[coop_tg_stayer].button != 0 || D_global_asm_807ECD10[coop_tg_stayer].stick_x > 30 || D_global_asm_807ECD10[coop_tg_stayer].stick_x < -30
            || D_global_asm_807ECD10[coop_tg_stayer].stick_y > 30 || D_global_asm_807ECD10[coop_tg_stayer].stick_y < -30);
        coop_tg_hold_t++;
        if (hk == NULL || (!busy && coop_tg_hold_t > 120 && moved) || coop_tg_hold_t > 3600) {
            coop_tg_hold = 0; // scene over and the player takes the controls (or safety limit)
        } else if (busy || coop_tg_hold_t <= 30) {
            hk->x_position = coop_tg_x;
            hk->y_position = coop_tg_y;
            hk->z_position = coop_tg_z;
        }
    }
    // the centred placement of the dance only lasts for the dance cutscene (not for the scene that follows it)
    if (arg0 == 1 && coop_gbd_on && is_cutscene_active == 0 && (u32)((u32)object_timer - coop_gbtp_t) > 30) coop_gbd_on = 0;
    // golden banana dance over: the Kong who was brought to the picker goes back where he was
    if (arg0 == 1 && coop_gbtp_on && coop_active_here() && character_change_array != NULL) {
        Actor *tq = character_change_array[coop_gbtp_who].playerPointer;
        u32 el = (u32)object_timer - coop_gbtp_t;
        if (is_cutscene_active != 0) coop_gbtp_quiet = 0; else coop_gbtp_quiet++;
        // phases: 1 = dance running, 2 = sent back, a following scene (Diddy's, 3 coconut switches) may still glue him to P1 again,
        // 3 = that scene is running: he is sent back once more when it is over
        if (tq == NULL || el > 1200 || (coop_gbtp_on == 2 && el > 700)) {
            coop_gbtp_on = 0;
        } else if (coop_gbtp_on == 2) {
            if (is_cutscene_active != 0) coop_gbtp_on = 3;
        } else if (el > 30 && coop_gbtp_quiet > 15 && (coop_gbtp_on == 3 || tq->control_state != 0x70)) {
            tq->x_position = coop_gbtp_x;
            tq->y_position = tq->unkA0 = coop_gbtp_y;
            tq->z_position = coop_gbtp_z;
            tq->y_rotation = tq->unkEE = coop_gbtp_ang;
            tq->y_velocity = 0.0f;
            coop_gbtp_on = (coop_gbtp_on == 1) ? 2 : 0;
        }
    }
    int tg_noglue = (current_map == MAP_TRAINING_GROUNDS && coop_tg_free);
    // Entrance cutscene / autowalk: P2 has no autowalk and would stand in the door (exit loading zone) while
    // the user can walk him out of the level. Keep P2 glued next to P1 until P1 regains control.
    if (arg0 == 1 && coop_active_here() && character_change_array != NULL) {
        Actor *c0 = character_change_array[0].playerPointer;
        if (coop_cannon_cam_saved != NULL && (is_cutscene_active == 0 || coop_cannon_cam_map != (int)current_map)) { if (coop_cannon_cam_map == (int)current_map) D_global_asm_807F5D10 = coop_cannon_cam_saved; coop_cannon_cam_saved = NULL; }
        Actor *c1 = character_change_array[1].playerPointer;
        if (coop_arena_back > 0) {
            coop_arena_back--;
            if (c0 && character_change_array[1].playerPointer && coop_arena_back <= 30) { // (the first ticks: P1 is still being placed by the exit)
                Actor *q = character_change_array[1].playerPointer;
                s16 aa = (s16)c0->y_rotation;
                q->x_position = c0->x_position + func_global_asm_80612794(aa + 0x400) * 25.0f;
                q->z_position = c0->z_position + func_global_asm_80612790(aa + 0x400) * 25.0f;
                q->y_position = q->unkA0 = c0->y_position;
                q->y_rotation = q->unkEE = aa;
            }
        }
        // P2 walks out of a door: the exit scripts move P1 along the same path (his swirl), he must not stand in P2's way
        static int door_walk_was = 0;
        int door_walking = 0;
        if (c0 && c1 && is_autowalking == 3 && (current_map != MAP_TRAINING_GROUNDS || coop_tg_cave) && !(coop_tg_hold && current_map == coop_tg_map)) { // (TG: the cave from DK Isles, not the lab / barrel exits)
            if (coop_door_ret1 > 0 && D_global_asm_807FD710 == c0) door_walking = 1; // (the walk always belongs to P1; P2 runs beside him)
        }
        // (one more alignment after the walk: the walker still glides a few frames, the other one would stay behind)
        if (door_walking) door_walk_was = 12;
        else if (door_walk_was > 0) { door_walk_was--; if (coop_raw_btn[1] != 0) door_walk_was = 0; }
        // Hott&Stroff door, exit towards the world: P1 walks out alone; P2 copies nothing (no heading, no side, not glued): he is just put on the ground
        // outside the door, 40 units to the side of the exit point, and kept there until the exit is over (else the two Kongs walk back into the door)
        if (c0 && c1 && COOP_HS_EXIT && (door_walking || door_walk_was > 0 || is_cutscene_active != 0 || is_autowalking == 3)) {
            // (12.5 to his left of the exit point: P1 is shifted 12.5 to its right (ent_shift), so they are 25 apart and the pair is centred, like the other doors / caves; he is also put COOP_HS_AHEAD units in front of the door: the wall left of the door held him)
            c1->x_position = coop_door_ex + func_global_asm_80612794(coop_door_ang + 0x400) * COOP_ENTRY_CENTER + func_global_asm_80612794(coop_door_ang) * COOP_HS_AHEAD;
            c1->y_position = c0->y_position;
            c1->z_position = coop_door_ez + func_global_asm_80612790(coop_door_ang + 0x400) * COOP_ENTRY_CENTER + func_global_asm_80612790(coop_door_ang) * COOP_HS_AHEAD;
            c1->y_rotation = c1->unkEE = coop_door_ang + COOP_HS_TURN; // facing away from the door (COOP_HS_TURN 0x800 if he faces the door)
        }
        if (c0 && c1 && !COOP_HS_EXIT && (door_walking || (door_walk_was > 0 && coop_door_ret1 > 0))) {
            Actor *wk = c0; // walker (P1); the other Kong is kept beside him
            Actor *ot = c1;
            // (not during the swirl: both Kongs spin side by side like in the original)
            if (coop_door_hs && ot->control_state == 136) {
                // Hott&Stroff, P1 walks: the swirl sends P2 to the center of the door, which touches it again (loop). He stays at the exit point's side instead.
                ot->x_position = coop_door_ex + func_global_asm_80612794(coop_door_ang + 0x400) * 40.0f;
                ot->y_position = wk->y_position;
                ot->z_position = coop_door_ez + func_global_asm_80612790(coop_door_ang + 0x400) * 40.0f;
            } else
            // (swirl out, P2 walking: P1 is pinned beside him from the first frame to the end, seen from the exit angle since P2 spins on the spot)
            if (ot->control_state != 135 && ot->control_state != 136) {
                s16 ra = (ot->control_state == 136) ? coop_door_ang : (s16)wk->y_rotation;
                f32 dsd = 25.0f; // same side whoever walks: J2 stands at J1's left, and J1 at J2's left
                f32 sx = func_global_asm_80612794(ra + 0x400) * dsd, sz = func_global_asm_80612790(ra + 0x400) * dsd; // always the same side (the one of the placement), no flipping
                ot->x_position = wk->x_position + sx;
                ot->y_position = wk->y_position;
                ot->z_position = wk->z_position + sz;
                ot->y_rotation = wk->y_rotation; ot->unkEE = wk->y_rotation; // same heading as the walker (else he keeps the heading of the way in and runs toward the exit)
            }
        }
        // P2 walks into a door (swirl in, state 135 for both): the game sends both Kongs to the same point, P1 would turn inside P2.
        // He is kept at P2's right (25 units, side fixed when the swirl starts, P2 spins on the spot).
        {
            static int dep_on = 0;
            static s16 dep_ra = 0;
            if (c0 && c1 && coop_door_trig == 1 && current_map != MAP_TRAINING_GROUNDS && c0->control_state == 135 && c1->control_state == 135) {
                if (!dep_on) { dep_ra = (s16)c1->y_rotation; dep_on = 1; }
                c0->x_position = c1->x_position + func_global_asm_80612794(dep_ra + 0x400) * 25.0f;
                c0->y_position = c1->y_position;
                c0->z_position = c1->z_position + func_global_asm_80612790(dep_ra + 0x400) * 25.0f;
            } else dep_on = 0;
        }
        if (c1) {   // Japes cannon, P2 as the user. The launch (upward speed) is not given to P2 by the game, and P2 is dropped
            // out of the flight state (0x45 -> 0x1E) about every 20 frames while P1 keeps it until it lands. Log of P1 (works):
            // first frame of state 0x45 has y_velocity = 812, speed 206 (-1 per frame), flag unk64 = 0x80.
            static int cn_prev1 = 0;
            static f32 cn_spd = 0.0f;
            static int cn_anim = -1;
            static int cn_latch = 0; // frames left of the forced flight of P2 (0 = not flying)
            int st1 = c1->control_state;
            if (cn_latch > 0) {
                cn_latch--;
                if (cn_spd > 0.0f) cn_spd -= 1.0f;
                if (c1->control_state != 0x45) {
                    c1->control_state = 0x45;
                    c1->control_state_progress = 0;
                    c1->unk64 |= 0x80;
                    if ((int)c1->animation_state->unk64 != cn_anim) playAnimation(c1, 0x22); // the spin keeps playing if the kick did not change it
                    st1 = 0x45;
                }
                if (c1->unkB8 < cn_spd) c1->unkB8 = cn_spd;
                if (c1->y_velocity < 0.0f && c1->distance_from_floor < 25.0f) cn_latch = 0; // landing: back to normal
                // the cutscene is over and P2 is standing on the ground (vy is not always < 0 there): stop forcing the flight
                if (is_cutscene_active == 0 && c1->distance_from_floor < 8.0f && c1->y_velocity <= 0.0f) cn_latch = 0;
            }
            // The game only lets P2 leave the flight state (landing) when P1 stands still (it stayed "spinning" on the ground while
            // P1 was running). Landing is therefore done here: cutscene over + on the ground for 3 checks in a row -> idle.
            {
                static int cn_ground = 0;
                if (is_cutscene_active == 0 && c1->control_state == 0x45 && c1->distance_from_floor < 8.0f && c1->y_velocity <= 0.0f) {
                    if (++cn_ground >= 3) {
                        cn_ground = 0;
                        cn_latch = 0;
                        c1->control_state = 0xC;
                        c1->control_state_progress = 0;
                        c1->unk64 &= ~0x80;
                        c1->y_velocity = 0.0f;
                        playAnimation(c1, 9);
                        c1->x_rotation = 0;
                        c1->z_rotation = 0;
                        st1 = 0xC;
                    }
                } else {
                    cn_ground = 0;
                }
            }
            if (st1 == 0x45 && cn_prev1 != 0x45 && c1->y_velocity < 100.0f) {
                c1->y_velocity = 812.0f;
                c1->control_state_progress = 0;
                c1->unk64 |= 0x80;
                playAnimation(c1, 0x22);
                cn_anim = (int)c1->animation_state->unk64;
                cn_latch = 900;
                cn_spd = c1->unkB8;
            }
            cn_prev1 = st1;
        }
        if (c1 && current_map != MAP_JAPES_ARMY_DILLO && current_character_index[1] <= 4) {
            coop_p2_kong_mem = current_character_index[1];
        }
        // Door / lobby entry and exit "autowalk": the game only simulates the pad of player 0; the other pads keep their last
        // value, so P2 kept walking where his stick pointed just before the door. P2 repeats P1's autowalk input instead.
        if (character_change_array[0].unk294 == &coop_zero_pad && !(is_autowalking == 3 && D_global_asm_807FD710 == c1)) {
            character_change_array[0].unk294 = &D_global_asm_807ECD10[0];
        }
        if (c0 && c1 && is_autowalking == 3 && D_global_asm_807FD710 == c1) {
            // P2 is the one who walks (he entered / leaves a lab...): the autowalk simulates the pad of port 0, P2 takes it and P1 stands still
            if (character_change_array[0].unk294 != NULL && character_change_array[1].unk294 != NULL) {
                // (Klumsy: the walk path also presses buttons: the roll. P2 gets them too)
                character_change_array[1].unk294->button = (current_map == MAP_KLUMSY) ? D_global_asm_807ECD10[0].button : 0;
                character_change_array[1].unk294->stick_x = D_global_asm_807ECD10[0].stick_x;
                character_change_array[1].unk294->stick_y = D_global_asm_807ECD10[0].stick_y;
                // P1 stands still: he reads a neutral pad (the autowalk's pad 0 must stay untouched, its stick ramps up from its own previous value)
                // (door / cave / lobby between two co-op maps: P1 repeats the walk too, like P2 does when P1 walks; he stays beside P2, see below)
                character_change_array[0].unk294 = &coop_zero_pad;
            }
            if (character_change_array[1].new_controller_inputs != NULL) character_change_array[1].new_controller_inputs->button = (current_map == MAP_KLUMSY) ? newly_pressed_input[0] : 0;
        } else if (c0 && c1 && is_autowalking == 3 && !tg_noglue) {
            if (character_change_array[0].unk294 != NULL && character_change_array[1].unk294 != NULL) {
                character_change_array[1].unk294->button = (current_map == MAP_KLUMSY) ? character_change_array[0].unk294->button : 0;
                character_change_array[1].unk294->stick_x = character_change_array[0].unk294->stick_x;
                character_change_array[1].unk294->stick_y = character_change_array[0].unk294->stick_y;
            }
            if (character_change_array[1].new_controller_inputs != NULL) {
                character_change_array[1].new_controller_inputs->button = (current_map == MAP_KLUMSY) ? newly_pressed_input[0] : 0;
            }
        }
        // Training Grounds cave (from DK Isles): the common door walk logic (P2 beside the walker, same heading, see door_walking above) applies
        // while the walk lasts; this only tells when it is over
        if (current_map == MAP_TRAINING_GROUNDS && coop_tg_cave) {
            if (is_autowalking == 3) {
                coop_tg_cave_on = 1;
            } else if (coop_tg_cave_on) {
                coop_tg_cave = 0;
                coop_tg_cave_on = 0;
            }
        } else if (current_map != MAP_TRAINING_GROUNDS && current_map != MAP_DK_ISLES_OVERWORLD) {
            coop_tg_cave = 0;
            coop_tg_cave_on = 0;
        }
        // Japes arrival cutscene (18): it ends before the parrot's last lines. Like in the original game nobody moves until the speech bubble is
        // gone; the bubble reads its own pad copy (P1 can still skip / advance the text), so only the movement pads are neutralised.
        if (current_map != MAP_JAPES) coop_jp18 = 0;
        else if (is_cutscene_active == 1 && D_global_asm_807476F4 == 18) coop_jp18 = 1;
        else if (coop_jp18 && is_cutscene_active == 0 && ((int)object_timer < coop_txt_last || (int)object_timer - coop_txt_last > 3)) coop_jp18 = 0; // cutscene over and no bubble left
        if (c0 && c1 && coop_jp18 && (int)object_timer >= coop_txt_last && (int)object_timer - coop_txt_last <= 3) {
            int jk_;
            for (jk_ = 0; jk_ < 2; jk_++) {
                if (character_change_array[jk_].unk294 != NULL) {
                    character_change_array[jk_].unk294->button = 0;
                    character_change_array[jk_].unk294->stick_x = 0;
                    character_change_array[jk_].unk294->stick_y = 0;
                }
                if (character_change_array[jk_].new_controller_inputs != NULL) character_change_array[jk_].new_controller_inputs->button = 0;
            }
        }
        // P2's glue to P1 during cutscenes: at P1's side while a forced walk runs and until the cutscene ends, else at a fixed world offset
        if (is_autowalking == 3) coop_glue_side = 1;
        else if (is_cutscene_active == 0) coop_glue_side = 0;
        if (is_cutscene_active == 0) coop_cs_trig = -1;
        if (c0 && c1 && coop_barrel_chooser_now() >= 0) {
            // tag barrel selection: the other Kong stays where it is (no teleport), it just cannot move
            int oi = (coop_barrel_chooser_now() == 1) ? 0 : 1;
            // the waiting Kong cannot move: its controller is neutral (its movement shifted the scene camera)
            if (character_change_array[oi].unk294 != NULL) {
                character_change_array[oi].unk294->button = 0;
                character_change_array[oi].unk294->stick_x = 0;
                character_change_array[oi].unk294->stick_y = 0;
            }
            if (character_change_array[oi].new_controller_inputs != NULL) {
                character_change_array[oi].new_controller_inputs->button = 0;
            }
        } else if (c0 && c1 && (coop_cannon_user() >= 0 || (is_cutscene_active != 0 && current_map == MAP_JAPES && D_global_asm_807476F4 == 22) || (is_cutscene_active != 0 && (coop_barrel_user() == 1 || coop_enter_user() == 1)))) {
            // Japes cannon: no glue of P2 next to P1. The cutscene (camera actor D_global_asm_807F5D10, subject
            // D_global_asm_807F5CE8) is bound to P2's own camera when P2 uses the cannon, so P1's screen is untouched.
            if (is_cutscene_active != 0 && (coop_cannon_user() == 1 || coop_barrel_user() == 1 || coop_enter_user() == 1)) {
                Actor *cam1 = ((PlayerAdditionalActorData *)c1->PaaD)->unk104;
                if (cam1 != NULL && D_global_asm_807F5D10 != cam1) {
                    coop_cannon_cam_saved = D_global_asm_807F5D10;
                    coop_cannon_cam_map = (int)current_map;
                    D_global_asm_807F5D10 = cam1;
                }
                if (D_global_asm_807F5CE8 == c0) D_global_asm_807F5CE8 = c1;
                // Klumsy: the cutscene turns P1 (player 0); P2, standing beside him, keeps the same heading (otherwise he ends up facing away)
                if (current_map == MAP_KLUMSY && coop_isles_scene) {
                    f32 kdx = c1->x_position - c0->x_position, kdz = c1->z_position - c0->z_position;
                    // (last scene: the map is reloaded and P2 is put at the exit while the scene puts P1 elsewhere: P2 is brought beside him)
                    if (kdx * kdx + kdz * kdz > 6400.0f) {
                        c1->x_position = c0->x_position + 25.0f;
                        c1->y_position = c0->y_position;
                        c1->z_position = c0->z_position;
                    }
                    c1->y_rotation = c0->y_rotation; c1->unkEE = c0->y_rotation;
                }
            }
        } else if (c0 && c1 && is_cutscene_active == 1 && D_global_asm_807F5CE8 == c1 && c0->control_state != 0x88 && !coop_cs_noglue && !tg_noglue && !coop_death_cam() && !coop_barrel_sel && coop_feeder_query() != 1 && coop_cs_trig < 0 && !COOP_HS_EXIT) {
            // cutscene whose subject is P2 (golden banana dance of P2...): P1 joins P2, not the other way round
            c0->x_position = c1->x_position + 25.0f;
            c0->y_position = c1->y_position;
            c0->z_position = c1->z_position;
            c0->y_rotation = c1->y_rotation;
        } else if (c0 && c1 && (c0->control_state == 0x88 || (is_cutscene_active != 0 && !coop_cs_noglue)) && !tg_noglue && !coop_death_cam() && !coop_barrel_sel && coop_feeder_query() != 1 && coop_cs_trig < 0 && !COOP_HS_EXIT) {
            if (current_map == MAP_KLUMSY || coop_glue_side) {
                // P1 runs (walk of a door / cave / Klumsy...): P2 follows at P1's side, not at a fixed world offset (that put him in front of
                // P1 depending on the direction, they bumped each other). The side is kept until the end of the cutscene (no jump).
                c1->x_position = c0->x_position + func_global_asm_80612794((s16)c0->y_rotation + 0x400) * 25.0f;
                c1->z_position = c0->z_position + func_global_asm_80612790((s16)c0->y_rotation + 0x400) * 25.0f;
                c1->unkEE = c0->y_rotation;
            } else {
                c1->x_position = c0->x_position + 25.0f;
                c1->z_position = c0->z_position;
            }
            c1->y_position = c0->y_position;
            c1->y_rotation = c0->y_rotation;
        }
        // Golden banana dance scene: the pair stands side by side across the camera (the side is perpendicular to the heading, which faces the
        // camera), centred on the place where the banana was picked up (the scene camera looks at that point): picker half a step one way,
        // the other Kong half a step the other way.
        if (coop_gbd_on && is_cutscene_active != 0 && c0 && c1) {
            u32 gel = (u32)object_timer - coop_gbtp_t;
            Actor *gpk = character_change_array[coop_gbd_who].playerPointer;
            Actor *got = character_change_array[1 - coop_gbd_who].playerPointer;
            if (gpk == NULL || got == NULL || gel > 300) {
                coop_gbd_on = 0;
            } else {
                s16 gaa = (s16)gpk->y_rotation;
                // the side is taken from the heading while the dance starts, then kept until the end of the scene (when the dance is over the
                // glue of the cutscene would otherwise put the other Kong on the opposite side)
                if (!coop_gbd_have) {
                    coop_gbd_sx = func_global_asm_80612794(gaa + 0x400) * COOP_GBD_HALF;
                    coop_gbd_sz = func_global_asm_80612790(gaa + 0x400) * COOP_GBD_HALF;
                    if (gel >= 25 || gpk->control_state == 0x70) coop_gbd_have = 1;
                }
                f32 gsx = coop_gbd_sx;
                f32 gsz = coop_gbd_sz;
                gpk->x_position = coop_gbd_x - gsx;
                gpk->z_position = coop_gbd_z - gsz;
                got->x_position = coop_gbd_x + gsx;
                got->z_position = coop_gbd_z + gsz;
                gpk->y_position = gpk->unkA0 = coop_gbd_y;
                got->y_position = got->unkA0 = coop_gbd_y;
                got->y_rotation = got->unkEE = gaa;
            }
        }
    }
    for (D_global_asm_807FBB3C = 0; D_global_asm_807FBB3C < D_global_asm_807FBB34; D_global_asm_807FBB3C++) {
        D_global_asm_807FBE08 = 0;
        gCurrentActorPointer = D_global_asm_807FB930[D_global_asm_807FBB3C].unk0;
        if (((arg0 == 1) && ((gCurrentActorPointer->object_properties_bitfield & 0x100000) == 0)) ||
            ((arg0 == 2) && ((gCurrentActorPointer->object_properties_bitfield & 0x100000) != 0))
        ) {
            coop_tgt_swapped = 0;
            if (coop_active_here() && !(gCurrentActorPointer->interactable & 1) && !(gCurrentActorPointer->object_properties_bitfield & 0x100000) && COOP_P_ENEMY) {
                Actor *near_ = coop_nearest_kong(gCurrentActorPointer);
                if (near_ != NULL && near_ != character_change_array[0].playerPointer) {
                    gPlayerPointer = near_;
                    coop_tgt_swapped = 1;
                }
                // Training Grounds, scene after the last barrel / the lab: the parrot appears in front of the Kong who came back (P2 if he did)
                {
                    int sq_ = gCurrentActorPointer->unk58;
                    if (current_map == MAP_TRAINING_GROUNDS && is_cutscene_active != 0 && character_change_array[1].playerPointer != NULL
                        && (coop_enter_user() == 1 || (coop_tg_hold && coop_tg_stayer == 0))
                        && (sq_ == ACTOR_SQUAWKS || sq_ == ACTOR_SQUAWKS_0 || sq_ == ACTOR_SQUAWKS_1 || sq_ == ACTOR_SQUAWKS_2)) {
                        gPlayerPointer = character_change_array[1].playerPointer;
                        coop_tgt_swapped = 1;
                    }
                    // DK Isles arrival scene: the parrot talks to the Kong who walked out of the Training Grounds
                    if (coop_isles_scene && current_map == coop_isles_map && is_cutscene_active != 0 && character_change_array[coop_isles_who].playerPointer != NULL
                        && (sq_ == ACTOR_SQUAWKS || sq_ == ACTOR_SQUAWKS_0 || sq_ == ACTOR_SQUAWKS_1 || sq_ == ACTOR_SQUAWKS_2)) {
                        gPlayerPointer = character_change_array[coop_isles_who].playerPointer;
                        coop_tgt_swapped = 1;
                    }
                }
            }
            // The prop controller (map-object scripts) also reads data of the player given by cc_player_index (progress counters...): the player it follows
            if (coop_active_here() && gCurrentActorPointer->unk58 == ACTOR_PROP_CONTROLLER) {
                coop_cc_saved = cc_player_index;
                cc_player_index = (gPlayerPointer == character_change_array[1].playerPointer && character_change_array[1].does_player_exist) ? 1 : 0;
                coop_cc_swapped = 1;
            }
            func_global_asm_80678BBC(gCurrentActorPointer);
            {
                // Troff 'n' Scoff circle (pad event type 3): only the Kong the event was computed for may react to it
                // (P1's update used to take P2's event and start the feeding on P1 with a bad pad position).
                int gate_ = 0;
                int who_ = -1;
                __typeof__(D_global_asm_807FBB70.unk2) gate_saved_ = 0;
                if (character_change_array[0].playerPointer == gCurrentActorPointer) {
                    who_ = 0;
                } else if (character_change_array[1].does_player_exist && character_change_array[1].playerPointer == gCurrentActorPointer) {
                    who_ = 1;
                }
                if (coop_active_here() && current_map == MAP_TROFF_N_SCOFF && who_ >= 0
                    && D_global_asm_807FBB70.unk2 != 0 && D_global_asm_807FBB70.unk14 == 3 && who_ != coop_pad_owner) {
                    gate_saved_ = D_global_asm_807FBB70.unk2;
                    D_global_asm_807FBB70.unk2 = 0;
                    gate_ = 1;
                }
                // Speech bubbles (text clouds of cutscenes / mini-games): B skips, A speeds up, Z slows down. They read the pad of
                // player 0 only. When the scene belongs to P2 the bubble reads his pad instead.
                OSContPad tx_pad_ = D_global_asm_807ECD58;
                u16 tx_new_ = newly_pressed_input_copy;
                int tx_swap_ = 0;
                if ((void *)D_global_asm_8074C0A0[gCurrentActorPointer->unk58] == (void *)func_global_asm_806A5868) {
                    int src_ = -1;
                    if (coop_jp18 && current_map == MAP_JAPES) coop_txt_last = (int)object_timer; // a speech bubble is alive
                    if (coop_mg_swapped && !coop_map_allowed()) src_ = 0; // barrel mini-game played by P2: his pad was moved into port 0
                    else if (coop_active_here() && character_change_array[1].does_player_exist && is_cutscene_active != 0
                        && ((D_global_asm_807F5CE8 != NULL && D_global_asm_807F5CE8 == character_change_array[1].playerPointer) || coop_cs_trig == 1)) src_ = 1;
                    if (src_ >= 0) {
                        D_global_asm_807ECD58.button = coop_raw_btn[1];
                        newly_pressed_input_copy = coop_raw_new[1];
                        tx_swap_ = 1;
                    }
                }
                D_global_asm_8074C0A0[gCurrentActorPointer->unk58]();
                if (tx_swap_) { D_global_asm_807ECD58 = tx_pad_; newly_pressed_input_copy = tx_new_; }
                if (gate_) {
                    D_global_asm_807FBB70.unk2 = gate_saved_;
                }
            }
            
            
            func_global_asm_80678CC8(gCurrentActorPointer);
            if (coop_cc_swapped) { cc_player_index = coop_cc_saved; coop_cc_swapped = 0; }
            if (coop_tgt_swapped) {
                gPlayerPointer = character_change_array[0].playerPointer; // back to P1
                coop_tgt_swapped = 0;
            }
            
        }
    }
}

// ---- Story co-op: render loop (func_global_asm_8062BF24) ----
// Identical to the original, except that in co-op the viewport rectangle / aspect of each player is restored
// before drawing: the frame builder overwrites character_change_array[0].unk270 with the full-screen rectangle
// (HUD, cinematic bars), which made P1's viewport cover the whole screen.
extern void func_global_asm_8062C1C0(s32);
extern void func_global_asm_8062C22C(void);
extern f32 func_global_asm_8062A850(void);
extern u8 D_global_asm_807F5FED;

// @coop: segment 2 (D_2000000 = projection, D_2000200 = look-at) is the P1 "global camera". Several draw routines
// (sprites with alignment, special actors) reload it and assume it matches the current viewport. For each viewport we
// therefore point segment 2 to a copy of the frame matrix header whose projection / look-at are the player's own.
static u32 coop_seg2[2][2][0x240 / 4] __attribute__((aligned(16)));
static void *coop_frame_hdr = NULL; // frame header (segment 2) of the current frame, used by the P2 HUD
static u32 coop_hud_seg2[2][2][0x240 / 4] __attribute__((aligned(16))); // [player][frame]

// ---- Story co-op: pause menu drawn in the half of the screen of the player who pressed START ----
// The menu draws with the full-screen HUD ortho (segment 2 + 0xC0, 1/4 px units, 1280 x 960). Its pieces call
// func_global_asm_806A921C first; there segment 2 is pointed to a copy of the frame header whose ortho is scaled by 0.5 (uniform,
// so nothing is stretched) and placed in the middle of the pauser's half, and the scissor is limited to that half. The end of
// each menu piece calls coop_pause_half_end to put everything back.
static u32 coop_pause_seg2[2][2][0x240 / 4] __attribute__((aligned(16))); // [half][frame]
static int coop_pause_half_active = 0;
static int coop_pause_other = 0; // 1: set up the half of the player who did NOT pause ("EN PAUSE" caption)
void coop_pause_set_other(int on) { coop_pause_other = on; }
void *coop_pause_half(int *rect, f32 xk) {
    u32 *buf;
    u32 *src;
    int k, pl;
    coop_pause_half_active = 0;
    if (!coop_active_here() || !(global_properties_bitfield & 2) || coop_frame_hdr == NULL || !coop_vp_saved
        || !character_change_array[1].does_player_exist) return NULL;
    pl = (D_global_asm_807FC8B9 & 1) ^ (coop_pause_other ? 1 : 0);
    buf = coop_pause_seg2[pl][D_global_asm_807444FC & 1];
    src = (u32 *)coop_frame_hdr;
    for (k = 0; k < 0x240 / 4; k++) buf[k] = src[k];
    if (pl == 0) {
        guOrtho((Mtx *)&buf[0xC0 / 4], 639.0f - 1279.0f * xk, 639.0f + 1279.0f * xk, 1918.0f, 0.0f, -20000.0f, 20000.0f, 1.0f);
    } else {
        guOrtho((Mtx *)&buf[0xC0 / 4], 639.0f - 1279.0f * xk, 639.0f + 1279.0f * xk, 958.0f, -960.0f, -20000.0f, 20000.0f, 1.0f);
    }
    for (k = 0; k < 4; k++) rect[k] = coop_vp_rect[pl][k];
    coop_pause_half_active = 1;
    return buf;
}
int coop_pause_half_on(void) { return coop_pause_half_active; }
// 1 while the pause menu is shown in the half of the pauser's screen (used by the sprite update)
int coop_pause_half_wanted(void) {
    return coop_active_here() && (global_properties_bitfield & 2) && coop_frame_hdr != NULL && coop_vp_saved
        && character_change_array[1].does_player_exist;
}
void *coop_frame_hdr_get(void) { return coop_frame_hdr; }
Gfx *coop_pause_half_end(Gfx *dl) {
    if (coop_pause_half_active) {
        gDPPipeSync(dl++);
        gDPSetScissorFrac(dl++, G_SC_NON_INTERLACE, 0, 0, D_global_asm_80744490 * 4.0f, D_global_asm_80744494 * 4.0f);
        gSPSegment(dl++, 0x02, osVirtualToPhysical(coop_frame_hdr));
        coop_pause_half_active = 0;
    }
    return dl;
}
#define COOP_HP_SCALE 0.60f // size of the health bar (melons) in each split-screen half (1.0 = original)

RECOMP_PATCH Gfx *func_global_asm_8062BF24(Gfx *dl, s32 arg1) {
    u8 temp_s4;
    CoopVpEntry *temp_v0_2;
    s32 i;
    s32 ii;

    coop_tr_update();
    coop_frame_hdr = (void *)arg1;
    func_global_asm_8062C1C0(arg1);
    gDPPipeSync(dl++);
    if (D_global_asm_807F5FED != 0) {
        gDPPipelineMode(dl++, G_PM_1PRIMITIVE);
    } else {
        gDPPipelineMode(dl++, G_PM_NPRIMITIVE);
    }
    for (ii = 0; ii < D_global_asm_80750AB4; ii++) {
        i = ii;
        if (coop_vp_saved && coop_active_here() && i < 2) {
            int k;
            for (k = 0; k < 4; k++) character_change_array[i].unk270[k] = coop_vp_rect[i][k];
            character_change_array[i].unk278_arr[0] = coop_vp_rect[i][2] - coop_vp_rect[i][0];
            character_change_array[i].unk278_arr[1] = coop_vp_rect[i][3] - coop_vp_rect[i][1];
            character_change_array[i].unk280 = coop_vp_asp[i];
        }
        if (coop_active_here() && i < 2 && coop_fullscreen_now() >= 0) {
            // tag barrel selection / cutscene: the scene is the camera of the Kong who is choosing (its camera is put in the
            // "barrel" mode); only that viewport is drawn, on the whole screen like in 1-player
            if (i != coop_fullscreen_now()) continue;
            character_change_array[i].unk270[0] = 0;
            character_change_array[i].unk270[1] = 0;
            character_change_array[i].unk270[2] = 320;
            character_change_array[i].unk270[3] = 240;
            character_change_array[i].unk278_arr[0] = 320;
            character_change_array[i].unk278_arr[1] = 240;
            character_change_array[i].unk280 = (320.0f / 240.0f);
        } else if (coop_active_here() && i < 2 && coop_tr_player >= 0 && coop_vp_saved) {
            // split-screen transition: the line between the two views moves from the full-screen edge to its final place
            int y0 = (i == 0) ? 0 : coop_tr_split;
            int y1 = (i == 0) ? coop_tr_split : 240;
            character_change_array[i].unk270[0] = coop_vp_rect[i][0];
            character_change_array[i].unk270[1] = y0;
            character_change_array[i].unk270[2] = coop_vp_rect[i][2];
            character_change_array[i].unk270[3] = y1;
            character_change_array[i].unk278_arr[0] = coop_vp_rect[i][2] - coop_vp_rect[i][0];
            character_change_array[i].unk278_arr[1] = y1 - y0;
            character_change_array[i].unk280 = (f32)character_change_array[i].unk278_arr[0] / (f32)(y1 - y0);
        }
        if (character_change_array[i].does_player_exist) {
            u32 *coop_seg2_buf = NULL;
            cc_player_index = i;
            if (coop_active_here() && i < 2) {
                coop_seg2_buf =coop_seg2[i][D_global_asm_807444FC & 1];
                gSPSegment(dl++, 0x02, osVirtualToPhysical(coop_seg2_buf));
            }
            int cs_ = (coop_active_here() && i < 2 && coop_death_cam()) ? 0 : i;
            f32 cam_e[3], cam_a[3];
            cam_e[0] = character_change_array[cs_].unk21C; cam_e[1] = character_change_array[cs_].unk220; cam_e[2] = character_change_array[cs_].unk224;
            cam_a[0] = character_change_array[cs_].unk234; cam_a[1] = character_change_array[cs_].unk238; cam_a[2] = character_change_array[cs_].unk23C;
            if (coop_active_here() && i < 2 && character_change_array[i].playerPointer != NULL) {
                // Japes cannon: the Kong who is not in the cannon keeps playing; its viewport uses the camera it had just before the
                // cutscene, translated with the Kong (same view angle), whatever the cutscene does to the engine camera.
                static f32 snap_e[2][3], snap_a[2][3], snap_p[2][3];
                static u8 snap_ok[2];
                Actor *pl = character_change_array[i].playerPointer;
                int cannon_cs = is_cutscene_active == 1 && current_map == MAP_JAPES && D_global_asm_807476F4 == 22 && coop_cannon_user() >= 0;
                if (cannon_cs && i != coop_cannon_user() && snap_ok[i]) {
                    int k;
                    f32 d[3];
                    d[0] = pl->x_position - snap_p[i][0]; d[1] = pl->y_position - snap_p[i][1]; d[2] = pl->z_position - snap_p[i][2];
                    for (k = 0; k < 3; k++) { cam_e[k] = snap_e[i][k] + d[k]; cam_a[k] = snap_a[i][k] + d[k]; }
                } else if (is_cutscene_active == 0) {
                    snap_e[i][0] = cam_e[0]; snap_e[i][1] = cam_e[1]; snap_e[i][2] = cam_e[2];
                    snap_a[i][0] = cam_a[0]; snap_a[i][1] = cam_a[1]; snap_a[i][2] = cam_a[2];
                    snap_p[i][0] = pl->x_position; snap_p[i][1] = pl->y_position; snap_p[i][2] = pl->z_position;
                    snap_ok[i] = 1;
                }
            }
            dl = func_global_asm_8062C29C(dl,
                cam_e[0],
                cam_e[1],
                cam_e[2],
                cam_a[0],
                cam_a[1],
                cam_a[2],
                character_change_array[cs_].unk28C,
                character_change_array[i].unk280 * func_global_asm_8062A850(),
                character_change_array[cs_].far
            );
            if (coop_seg2_buf != NULL) {
                int k;
                u32 *src = (u32 *)arg1;
                u32 *pm = (u32 *)&character_change_array[i].unk88[D_global_asm_807444FC];
                u32 *vm = (u32 *)&character_change_array[i].unk8[D_global_asm_807444FC];
                for (k = 0; k < 0x240 / 4; k++) coop_seg2_buf[k] = src[k];
                for (k = 0; k < 16; k++) coop_seg2_buf[k] = pm[k];
                for (k = 0; k < 16; k++) coop_seg2_buf[0x200 / 4 + k] = vm[k];
            }
            D_global_asm_807F5FEC = 0;
        }
        if ((character_change_array[i].does_player_exist == 0) || (character_change_array[i].unk2EA != 0)) {
            temp_s4 = character_change_array[i].playerPointer->shadow_opacity;
            temp_v0_2 = func_global_asm_806C7C94(i);
            gSPDisplayList(dl++, &D_1000118);
            gSPTexture(dl++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
            gDPPipeSync(dl++);
            gDPSetPrimColor(dl++, 0, 0, 0x00, 0x00, 0x00, 0xFF - temp_s4);
            gDPSetCombineMode(dl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            gDPFillRectangle(dl++,
                temp_v0_2->unk4,
                temp_v0_2->unk6,
                temp_v0_2->unk8,
                temp_v0_2->unkA
            );
            gDPPipeSync(dl++);
        }
    }
    if (coop_active_here()) {
        gSPSegment(dl++, 0x02, osVirtualToPhysical((void *)arg1)); // back to the normal frame header
    }
    func_global_asm_8062C22C();
    gDPPipeSync(dl++);
    return dl;
}

// ---- Story co-op: keep each player's aspect ratio after the frame builder ----
// func_global_asm_805FD088 (frame builder) draws the cinematic bars (intro / swirl) and rewrites P1's
// aspect (character_change_array[0].unk280) from the full-screen size AFTER the viewports were drawn. The camera
// update of the NEXT frame (guPerspective in func_global_asm_805FCA94) then builds P1's projection with it, which
// stretched P1's view during cinematics. func_global_asm_80722280 is the very last call of the frame builder
// (original body: two byte resets), so the aspect is restored here.
extern u8 D_global_asm_807FDB74;
extern u8 D_global_asm_807FDB75;
RECOMP_PATCH void func_global_asm_80722280(void) {
    D_global_asm_807FDB74 = 0;
    D_global_asm_807FDB75 = 0;
    if (coop_active_here() && coop_vp_saved) {
        int k;
        for (k = 0; k < 2; k++) {
            character_change_array[k].unk280 = coop_vp_asp[k];
        }
        // tag barrel scene drawn full screen: the projection of the next frame (built from unk280 by the camera update)
        // must use the full-screen aspect, not the split-screen one (which made everything too thin)
        if (coop_fullscreen_now() >= 0) {
            int ch_ = coop_fullscreen_now();
            character_change_array[ch_].unk280 = (320.0f / 240.0f);
        }
        // split-screen transition: the projection of the next frame (camera update) must match the viewport rectangle of that
        // frame, otherwise the picture is stretched while the line between the two views moves
        if (coop_tr_player >= 0) {
            u32 age_ = (u32)((u32)object_timer - coop_tr_start) + 1;
            u32 dur_ = coop_tr_mode ? COOP_TR_TICKS_IN : COOP_TR_TICKS;
            int sp_ = coop_tr_split_at(age_);
            int h0_ = sp_, h1_ = 240 - sp_;
            if (age_ < dur_) {
                character_change_array[0].unk280 = (f32)(coop_vp_rect[0][2] - coop_vp_rect[0][0]) / (f32)h0_;
                character_change_array[1].unk280 = (f32)(coop_vp_rect[1][2] - coop_vp_rect[1][0]) / (f32)h1_;
            } else if (coop_tr_mode) {
                character_change_array[coop_tr_player].unk280 = (320.0f / 240.0f);
            }
        }
    }
}

// ---- Story co-op: owner of deferred pickups ----
// In story mode (cc_number_of_players == 1) most pickups (melon slices, coins...) are not applied when touched but
// queued (func_global_asm_806F53EC) and applied later by func_global_asm_806F5378 as "player 0". The queue entry now
// remembers which Kong touched it (spare byte unk1B), and the later application runs with that owner.
typedef struct CoopQ36 CoopQ36;
struct CoopQ36 {
    s16 unk0;
    s16 unk2;
    s32 unk4;
    s32 unk8;
    s32 unkC;
    u8 unk10;
    u8 unk11;
    u8 unk12;
    u8 unk13;
    f32 unk14;
    u8 unk18;
    u8 unk19;
    u8 unk1A;
    u8 unk1B;
    CoopQ36 *next;
};
typedef struct {
    s16 unk0;
    s16 unk2;
    s32 unk4;
    s32 unk8;
    s32 unkC;
    s16 unk10;
    u8 unk12;
    u8 unk13;
    f32 unk14;
    u8 unk18;
    u8 unk19;
    u8 unk1A;
    u8 unk1B;
} CoopQ37;
extern void _free(void *);
extern void func_global_asm_806355DC(s32, s32);
extern void func_global_asm_806F54E0(u8, s32, u8);
extern CoopQ36 *D_global_asm_807FD730;
extern u8 D_global_asm_807FD738;

RECOMP_PATCH CoopQ36 *func_global_asm_806F53EC(CoopQ37 *arg0) {
    CoopQ36 *temp_v0 = _malloc(sizeof(CoopQ36));
    temp_v0->unk10 = 0;
    temp_v0->unk14 = 0.0f;
    temp_v0->unk0 = arg0->unk0;
    temp_v0->unk18 = arg0->unk10;
    temp_v0->unk2 = arg0->unk2;
    temp_v0->unk19 = arg0->unk12;
    temp_v0->unk1A = arg0->unk13;
    temp_v0->unk1B = coop_pickup_player;
    if (arg0->unk2 == 0x57 && recomp_get_coop_enabled() && coop_map_allowed()) {
        coop_melon_owner = coop_pickup_player;
        coop_melon_pending = 1;
    }
    if (recomp_get_coop_enabled() && coop_map_allowed()) {
    }
    return temp_v0;
}

RECOMP_PATCH void func_global_asm_806F5378(void) {
    CoopQ36 *next;
    CoopQ36 *current;

    D_global_asm_807FD738 = TRUE;
    current = D_global_asm_807FD730;
    while (current) {
        next = current->next;
        if (recomp_get_coop_enabled() && coop_map_allowed()) {
            coop_pickup_player = current->unk1B == 1 ? 1 : 0;
        }
        if (recomp_get_coop_enabled() && coop_map_allowed()) {
        }
        func_global_asm_806F54E0(0, current->unk2, current->unk1A);
        coop_pickup_player = 0;
        func_global_asm_806355DC(current->unk0, 1);
        _free(current);
        current = next;
    }
    D_global_asm_807FD730 = NULL;
}

// ---- Story co-op: P2 health HUD (melons) ----
// The original draws the health HUD only for i < cc_number_of_players (1 in co-op), so P2's pending damage was never
// applied and nothing was shown. In co-op P2's HUD is drawn with the same code, in P2's half of the screen: the HUD
// uses the full-screen ortho matrix at segment 2 + 0xC0, so for P2 segment 2 points to a copy of the frame header whose
// ortho matrix is shifted down by half the screen height.
#define COOP_P_HUD 1
extern Gfx *func_global_asm_80708DA4(Gfx *dl, s16 arg1, CharacterChange *arg2);

RECOMP_PATCH Gfx *func_global_asm_80709344(Gfx *dl) {
    s16 i;
    s16 n = cc_number_of_players;
#if COOP_P_HUD
    if (coop_active_here()) {
        n = 2;
    }
#endif
    for (i = 0; i < n; i++) {
        if (coop_active_here() && (coop_fullscreen_now() >= 0 || coop_tr_player >= 0)) continue; // full-screen scene / split transition: no HUD
        if (character_change_array[i].does_player_exist) {
            if (!(character_change_array[i].unk2DC.unk6 & 0x20)
            && (((character_change_array[i].unk2DC.unk8
                || character_change_array[i].unk2DC.unk6)
                && (D_global_asm_80744504 == 0))
                || (character_change_array[i].unk2DC.unk6 & 0x10))) {
#if COOP_P_HUD
                if (coop_active_here() && coop_frame_hdr != NULL) {
                    u32 *buf = coop_hud_seg2[i][D_global_asm_807444FC & 1];
                    u32 *src = (u32 *)coop_frame_hdr;
                    f32 hs = COOP_HP_SCALE;
                    int k;
                    for (k = 0; k < 0x240 / 4; k++) buf[k] = src[k];
                    // Full-screen HUD ortho (1/4 px units, 1280 x 960): the health bar is top-centre. Scale it around the
                    // top-centre of the player's half: P1 = top half (y 0), P2 = bottom half (y shifted down by 120 px).
                    if (i == 0) {
                        guOrtho((Mtx *)&buf[0xC0 / 4], 640.0f - 640.0f / hs, 640.0f + 639.0f / hs, 959.0f / hs, 0.0f, -20000.0f, 20000.0f, 1.0f);
                    } else {
                        guOrtho((Mtx *)&buf[0xC0 / 4], 640.0f - 640.0f / hs, 640.0f + 639.0f / hs, 479.0f / hs, -480.0f / hs, -20000.0f, 20000.0f, 1.0f);
                    }
                    // Back to the full-screen HUD state (viewport...): the first-person (aiming) overlay leaves the viewport of the aiming Kong's half
                    // set, which flattened the health bar of the aiming Kong and pushed the other Kong's one out of the screen.
                    dl = func_global_asm_805FD030(dl);
                    gSPSegment(dl++, 0x02, osVirtualToPhysical(buf));
                    // clip the HUD to the player's half of the screen (its slide animation would otherwise cross into the other view)
                    gDPPipeSync(dl++);
                    gDPSetScissorFrac(dl++, G_SC_NON_INTERLACE, coop_vp_rect[i][0] * 4.0f, coop_vp_rect[i][1] * 4.0f, coop_vp_rect[i][2] * 4.0f, coop_vp_rect[i][3] * 4.0f);
                    dl = func_global_asm_80708DA4(dl, i, &character_change_array[i]);
                    gDPPipeSync(dl++);
                    gDPSetScissorFrac(dl++, G_SC_NON_INTERLACE, 0, 0, D_global_asm_80744490 * 4.0f, D_global_asm_80744494 * 4.0f);
                    gSPSegment(dl++, 0x02, osVirtualToPhysical(coop_frame_hdr));
                    continue;
                }
#endif
                dl = func_global_asm_80708DA4(dl, i, &character_change_array[i]);
            }
        }
    }
    return dl;
}

// ---- Story co-op: per-player camera initialisation (original loops over cc_number_of_players, which stays 1) ----
extern void func_global_asm_8061EB04(Actor *, u8);

RECOMP_PATCH void func_global_asm_8061EA78(void) {
    u8 playerIndex;
    u8 count = coop_active_here() && D_global_asm_80750AC0 >= 2 ? 2 : cc_number_of_players;
    for (playerIndex = 0; playerIndex < count; playerIndex++) {
        if (character_change_array[playerIndex].does_player_exist) {
            func_global_asm_8061EB04(character_change_array[playerIndex].playerPointer, playerIndex);
        }
    }
}

// ---- Story co-op: map change request (copy of the original) ----
extern s16 D_global_asm_8076AEF2;
extern u8 D_global_asm_8076A0B2;
extern u8 D_global_asm_807444F8;
extern s8 loading_zone_transition_type;

extern void func_global_asm_806F5378(void);
extern s32 func_global_asm_80712798(void);
extern void func_global_asm_80712F10(s32 *, s32 *);
extern s32 func_global_asm_806C8DE0(s32);
extern void func_global_asm_805FF168(void);
extern void playSong(s32, f32);
extern void func_global_asm_807313BC(void);
extern void func_global_asm_805FF5A0(s32);
extern void func_boss_8002920C(void);

RECOMP_PATCH void func_global_asm_805FF378(s32 nextMap, s32 nextExit) {
    func_global_asm_806F5378();
    if (is_cutscene_active == 6 || !func_global_asm_80712798()) {
        func_global_asm_80712F10(&nextMap, &nextExit);
        if (nextMap != -1) {
            if (current_character_index[0] >= 6) {
                current_character_index[0] = func_global_asm_806C8DE0(0);
            }
            coop_door_p2 = (coop_door_trig == 1 && coop_active_here()) ? 1 : 0;
            // hall / cave / lobby zone: the Kong that walked in is the one running (state 13), the other one stands (12)
            if (coop_door_trig < 0 && coop_active_here() && character_change_array != NULL && character_change_array[1].does_player_exist
                && character_change_array[0].playerPointer != NULL && character_change_array[1].playerPointer != NULL) {
                // zone (hall / cave / lobby): the Kong closest to the zone walked in (log: 3541 vs 327551)
                Actor *z0 = character_change_array[0].playerPointer, *z1 = character_change_array[1].playerPointer;
                f32 dz0 = SQ(z0->x_position - D_global_asm_807FDCB8) + SQ(z0->z_position - D_global_asm_807FDCBC);
                f32 dz1 = SQ(z1->x_position - D_global_asm_807FDCB8) + SQ(z1->z_position - D_global_asm_807FDCBC);
                coop_door_p2 = (dz1 < dz0) ? 1 : 0;
            }
            coop_arrive_pending = coop_active_here() ? 1 : 0;
            coop_door_hs = (current_map == MAP_TROFF_N_SCOFF || nextMap == MAP_TROFF_N_SCOFF) ? 1 : 0;
            // Troff 'n' Scoff door (Hott&Stroff), both ways: P1 runs the exit walk like in the original (P2's walk looped the door)
            if (current_map == MAP_TROFF_N_SCOFF || nextMap == MAP_TROFF_N_SCOFF) coop_door_p2 = 0;
            coop_door_p1 = (!coop_door_p2 && coop_active_here()) ? 1 : 0;
            int shop_p2 = coop_door_p2; // (Kong who walked into the zone: the shops are played by him)
            // DK Isles: the stored zone position is that of the last trigger (music zones...), not the shop door: ask the zone's own logic
            // (state 13, else the Kong nearest to the zone actor = gPlayerPointer)
            if (coop_active_here() && current_map == MAP_DK_ISLES_OVERWORLD && coop_is_shop(nextMap) && character_change_array != NULL
                && character_change_array[1].does_player_exist && character_change_array[1].playerPointer != NULL)
                shop_p2 = (coop_zone_player() == character_change_array[1].playerPointer) ? 1 : 0;
            if (coop_isles_scene && coop_isles_trip && coop_active_here()
                && ((current_map == MAP_KLUMSY && nextMap == MAP_DK_ISLES_OVERWORLD && is_cutscene_active != 0)
                    || (current_map == MAP_DK_ISLES_OVERWORLD && nextMap == MAP_KLUMSY))) {
                // Klumsy's sequence goes to DK Isles (cutscene showing the first world's hall opening) and comes back: same scene, same Kong
                coop_isles_map = nextMap;
                coop_isles_from = current_map;
                if (nextMap == MAP_KLUMSY) coop_isles_ret = 1;
            } else {
                coop_isles_scene = (coop_active_here() && ((current_map == MAP_TRAINING_GROUNDS && nextMap == MAP_DK_ISLES_OVERWORLD)
                    || (current_map == MAP_DK_ISLES_OVERWORLD && nextMap == MAP_KLUMSY))) ? 1 : 0;
                coop_isles_trip = (coop_isles_scene && nextMap == MAP_KLUMSY) ? 1 : 0;
                coop_isles_map = nextMap;
                coop_isles_from = current_map;
                coop_isles_who = 0; // arrival scenes (parrot of the Training Grounds, Klumsy's roll...) are always J1's: screen, camera, text. (Was: the Kong who entered.)
                coop_isles_seen = 0; coop_isles_idle = 0; coop_isles_t = 0; coop_isles_cs = 0; coop_isles_ret = 0;
            }
            // Klumsy: the cutscene walk paths only work with P1 as the walker; P2 (even if he is the one who walked in) is placed beside him
            if (nextMap == MAP_KLUMSY && coop_active_here()) { coop_door_p2 = 0; coop_door_p1 = 1; }
            // Same for the cave of the Training Grounds (from DK Isles): with P2 as the walker he ran in circles behind the switch, the Training Grounds
            // rules (P1's pad neutral, no placement beside the walker, no glue) only suit the lab / barrel exits
            if (nextMap == MAP_TRAINING_GROUNDS && current_map == MAP_DK_ISLES_OVERWORLD && coop_active_here()) { coop_door_p2 = 0; coop_door_p1 = 1; coop_tg_cave = 1; coop_tg_cave_on = 0; }
            // EXITS (arrivals) are always walked by P1, whoever entered: P2 follows beside him (25 units to his left, same heading and states), the walker
            // being shifted by COOP_ENTRY_CENTER so that the pair is centred on the doorway. Who ENTERED (coop_door_trig, coop_isles_who) still decides the
            // entry fades and the scenes. One single case to maintain instead of one per Kong.
            if (coop_active_here()) { coop_door_p2 = 0; coop_door_p1 = 1; }
            coop_door_trig = -1;
            coop_mg_pending = 0;
            int shop_go = coop_active_here() && coop_is_shop(nextMap) && character_change_array != NULL
                && character_change_array[1].does_player_exist && character_change_array[0].playerPointer != NULL && character_change_array[1].playerPointer != NULL;
            coop_sh_go = shop_go; // (the two-player test of the shop's own load would see the single-player setting of that map)
            if (shop_go && shop_p2) {
                coop_mg_pending = 1; // P2 goes in: his persona, colour and pad (text and choices) are the shop's
                coop_mg_col = coop_col_idx[1];
            }
            if (current_map == MAP_TRAINING_GROUNDS && coop_active_here() && character_change_array != NULL && character_change_array[1].does_player_exist
                && nextMap != MAP_TRAINING_GROUNDS && (coop_barrel_user() == 1 || (coop_barrel_user() < 0 && coop_pad_owner == 1))) {
                // the player who enters (barrel: the one in the barrel state; lab / door: the one the zone's trigger was computed for)
                coop_mg_pending = 1;
                coop_mg_col = coop_col_idx[1];
            }
            // exit towards DK Isles (a co-op map, nothing to play): the one who walks out is the Kong closest to the zone, like halls / doors
            if (current_map == MAP_TRAINING_GROUNDS && nextMap == MAP_DK_ISLES_OVERWORLD && coop_active_here()) coop_mg_pending = coop_door_p2;
            if (((current_map == MAP_TRAINING_GROUNDS && nextMap != MAP_TRAINING_GROUNDS) || shop_go) && coop_active_here() && character_change_array != NULL
                && character_change_array[1].does_player_exist && character_change_array[1].playerPointer != NULL) {
                int stay = coop_mg_pending ? 0 : 1; // the barrel player leaves, the other one stays
                Actor *sp2 = character_change_array[stay].playerPointer;
                coop_tg_stayer = stay;
                coop_tg_map = current_map;
                coop_tg_x = sp2->x_position; coop_tg_y = sp2->y_position; coop_tg_z = sp2->z_position;
                coop_tg_rot = (s16)sp2->y_rotation;
                coop_tg_saved = 1;
                coop_tg_hold = 1;
                coop_tg_hold_t = 0;
                coop_tg_fade = stay ? 0 : 1; // the other Kong is the one who goes in
            }
            D_global_asm_8076A0B1 |= 1;
            D_global_asm_8076AEF2 = current_map;
            next_map = nextMap;
            next_exit = nextExit;
            D_global_asm_8076A0B2 = 3;
            func_global_asm_805FF168();
            loading_zone_transition_speed = 1.0f;
            if (D_global_asm_807444F8 != 1 && loading_zone_transition_type == 1 && !(D_global_asm_807FBB68 & 2) && current_map != MAP_DK_RAP) {
                playSong(MUSIC_44_DK_TRANSITION_CLOSING, 1.0f);
            }
            func_global_asm_807313BC();
            func_global_asm_805FF5A0(nextMap);
            if (D_global_asm_807FBB64 & 1) {
                func_boss_8002920C();
            }
        }
    }
}

// ---- Story co-op: copies of the "falling" (30) and "idle" (12) control state handlers ----
extern s32 handleInputsForControlState(s32);
extern void func_global_asm_806D3608(void);
extern void setYAccelerationFrom80753578(void);
extern void func_global_asm_806CC8B8(void);
extern void renderActor(Actor *, u8);
extern s32 func_global_asm_806CDD24(Actor *, f32, f32, s32);

RECOMP_PATCH void func_global_asm_806D36CC(void) {
    switch (gCurrentActorPointer->control_state_progress) {
        case 0:
            handleInputsForControlState(0x20);
            func_global_asm_806D3608();
            break;
        case 1:
            handleInputsForControlState(0x31);
            setYAccelerationFrom80753578();
            break;
    }
    func_global_asm_806CC8B8();
    renderActor(gCurrentActorPointer, 0);
}

RECOMP_PATCH void func_global_asm_806D2DC4(void) {
    s32 newControlState;
    s32 phi_a1;

    gCurrentActorPointer->unkEE = gCurrentActorPointer->y_rotation;
    gCurrentActorPointer->unk64 |= 0x200;
    phi_a1 = 0x13;
    if (gCurrentActorPointer->control_state_progress != 0) {
        gCurrentActorPointer->control_state_progress--;
        phi_a1 = 0x14;
    }
    if (object_timer > 1U && !handleInputsForControlState(phi_a1)) {
        newControlState = func_global_asm_806CDD24(gCurrentActorPointer, gCurrentActorPointer->unkB8, extra_player_info_pointer->unk4, 1);
        if (newControlState != 0xC) {
            gCurrentActorPointer->control_state = (u8)newControlState & 0xFF;
            gCurrentActorPointer->control_state_progress = 0;
        }
    }
    func_global_asm_806CC8B8();
    renderActor(gCurrentActorPointer, 0);
}

// ---- Story co-op: spawn placement of a player at the map exit (func_global_asm_806C90C4) ----
// The original also starts the exit "autowalk" (global state: walking path, camera script, input override).
// With two Kongs it was started twice (the 2nd call re-targeted the global autowalk to P2 and clobbered the flags),
// so the intro autowalk never ended: P1 kept walking by itself and no controls worked. In co-op only P1 runs the
// autowalk set-up; P2 is just placed at the exit.
typedef struct {
    s16 x, y, z;
    u8 angle;
    u8 unk7;
    u8 has_autowalk;
    u8 size;
} CoopExitData;
extern CoopExitData *getExitData(s32);
extern void func_global_asm_80614D00(Actor *, f32, f32);
extern void func_global_asm_8067ACB4(Actor *);
extern void func_global_asm_806F39E8(Actor *, CoopExitData *, u8);
extern u8 D_global_asm_80770DC9;
extern s16 D_global_asm_8076AEE2;
extern f32 func_global_asm_80612794(s16);
extern f32 func_global_asm_80612790(s16);
#define COOP_ENTRY_FWD 0.0f   // P2 start offset ahead of P1 on walk-in entries (door / lobby)
#define COOP_ENTRY_SIDE 25.0f // P2 start offset to the side of P1 on walk-in entries
#define COOP_P2_ENTRY 1 // 1 = P2 gets an entry placement/animation beside P1, 0 = P2 just placed at the exit

RECOMP_PATCH void func_global_asm_806C90C4(s32 exitIndex) {
    PlayerAdditionalActorData *PaaD;
    CoopExitData *exit;
    int is_p2 = coop_active_here() && ((PlayerAdditionalActorData *)gCurrentPlayer->PaaD)->unk1A4 == 1;

    PaaD = gCurrentPlayer->PaaD;
    exit = getExitData(exitIndex);
    if (!is_p2) {
        D_global_asm_8076AEE2 = 0;
        global_properties_bitfield &= -0x401;
    }
    // Training Grounds, back from a barrel / the lab. The exit data given to the 1st player (slot 0) is the barrel position; the
    // 2nd call gets the normal zone exit. Kept for P2 when he is the one who came out of the barrel.
    static CoopExitData tg_exit_stash;
    if (coop_active_here()) { coop_door_ex = exit->x; coop_door_ez = exit->z; coop_door_ang = (s16)((exit->angle / 255.0) * 4095.0); }
    int tg_ret = coop_active_here() && current_map == coop_tg_map && coop_tg_saved;
    if (tg_ret && !is_p2) tg_exit_stash = *exit;
    // Cave / hall / world door exits (forced walk): the Kong who walked in starts a little to the right so that the pair (the other Kong stands
    // at his left) is centred on the doorway. Not for Hott&Stroff, Klumsy and the scenes of coop_isles_*.
    static CoopExitData ent_ex;
    // Every type of exit is handled (walk, appear, landing): offsets along the "left" axis of the exit for P1 / P2 (ent_p1 / ent_p2) and a shifted
    // copy of the exit for the walk (ent_ex). P1 walks (shifted right of the exit), P2 stands at his left.
    int ent_shift = coop_active_here() && !tg_ret && coop_door_ret1 > 0 && (!coop_door_hs || current_map != MAP_TROFF_N_SCOFF) && current_map != MAP_KLUMSY; // (Hott&Stroff: only the exit into a world is centred, not the arrival inside the house)
    f32 ent_p1 = 0.0f, ent_p2 = 0.0f;
    if (ent_shift) {
        ent_p1 = -COOP_ENTRY_CENTER; ent_p2 = COOP_ENTRY_CENTER; // P1 walks (shifted to the right of the exit), P2 stands at his left
    }
    if (ent_shift && exit->has_autowalk == 1) {
        s16 ent_ang = (exit->angle / 255.0) * 4095.0;
        ent_ex = *exit;
        ent_ex.x = exit->x - func_global_asm_80612794(ent_ang + 0x400) * COOP_ENTRY_CENTER;
        ent_ex.z = exit->z - func_global_asm_80612790(ent_ang + 0x400) * COOP_ENTRY_CENTER;
    }
    if (tg_ret && is_p2) {
        coop_tg_saved = 0; // slot 1 is the last one placed
        if (coop_tg_stayer == 0) exit = &tg_exit_stash; // P2 came out of the barrel: he takes the barrel position
        if (coop_tg_stayer == 0 && exit->has_autowalk == 1) {
            // P2 came out of the lab: he runs the exit walk (the autowalk is bound to him, see func_global_asm_8067889C)
            coop_tg_free = 1;
            gCurrentPlayer->unkAC = -10000.0f;
            func_global_asm_806F39E8(gCurrentPlayer, exit, 0x32);
            gCurrentPlayer->unk6A &= ~0x200;
            PaaD->unk104->unk15F = exit->unk7 <= 0 ? 1 : exit->unk7;
            return;
        }
    }
    if (coop_active_here() && current_map == coop_tg_map && tg_ret && (is_p2 ? 1 : 0) == coop_tg_stayer) {
        // back from Cranky's lab / a barrel: the player who stayed is where he was, standing
        coop_tg_free = 1;
        gCurrentPlayer->unkAC = -10000.0f;
        gCurrentPlayer->x_position = coop_tg_x;
        gCurrentPlayer->y_position = gCurrentPlayer->unkA0 = coop_tg_y;
        gCurrentPlayer->z_position = coop_tg_z;
        gCurrentPlayer->y_rotation = coop_tg_rot;
        gCurrentPlayer->unkEE = coop_tg_rot;
        gCurrentPlayer->unk6A &= ~0x200;
        PaaD->unk104->unk15F = exit->unk7 <= 0 ? 1 : exit->unk7;
        return;
    }
#if COOP_P2_ENTRY
    // P2: the walk-in autowalk is a single global driver (one actor), so P2 cannot run it at the same time.
    // The "falling/landing" entry (case 2) is per-actor and is played for P2 too. For the walk-in (case 1), P2 is
    // placed beside P1 at the end of the walk path so both Kongs end up side by side.
    if (is_p2 && exit->has_autowalk == 2) {
        gCurrentPlayer->control_state = 0x4F;
        gCurrentPlayer->control_state_progress = 3;
        func_global_asm_80614D00(gCurrentPlayer, 0.5f, 0);
        PaaD->unk50 = 1;
        gCurrentPlayer->unk6A |= 4;
        func_global_asm_8067ACB4(gCurrentPlayer);
        playAnimation(gCurrentPlayer, 0x34);
    }
    if (is_p2) {
        s16 ang = (exit->angle / 255.0) * 4095.0;
        f32 fwd = exit->has_autowalk == 1 ? COOP_ENTRY_FWD : 0.0f;
        f32 side = exit->has_autowalk == 1 ? COOP_ENTRY_SIDE : 35.0f;
        if (ent_shift) side = ent_p2; // (the walker is moved to the right, P2 stands where the shift puts him)
        if (tg_ret && coop_tg_stayer == 0) { side = 0.0f; fwd = 0.0f; } // P2 came out of the barrel: on the exit
        gCurrentPlayer->unkAC = -10000.0f;
        gCurrentPlayer->x_position = exit->x + func_global_asm_80612794(ang) * fwd + func_global_asm_80612794(ang + 0x400) * side;
        gCurrentPlayer->y_position = gCurrentPlayer->unkA0 = exit->y;
        gCurrentPlayer->z_position = exit->z + func_global_asm_80612790(ang) * fwd + func_global_asm_80612790(ang + 0x400) * side;
        gCurrentPlayer->y_rotation = ang;
        gCurrentPlayer->unkEE = ang;
        gCurrentPlayer->unk6A &= ~0x200;
        PaaD->unk104->unk15F = exit->unk7 <= 0 ? 1 : exit->unk7;
        return;
    }
#endif
    switch (is_p2 ? 0 : exit->has_autowalk) {
        case 2:
            gCurrentPlayer->control_state = 0x4F;
            gCurrentPlayer->control_state_progress = 3;
            func_global_asm_80614D00(gCurrentPlayer, 0.5f, 0);
            PaaD->unk50 = 1;
            gCurrentPlayer->unk6A |= 4;
            func_global_asm_8067ACB4(gCurrentPlayer);
            playAnimation(gCurrentPlayer, 0x34);
            // Fallthrough
        case 0:
            gCurrentPlayer->unkAC = -10000.0f;
            gCurrentPlayer->y_rotation = (exit->angle / 255.0) * 4095.0;
            gCurrentPlayer->x_position = exit->x + func_global_asm_80612794((s16)gCurrentPlayer->y_rotation + 0x400) * ent_p1; // (ent_p1 = 0 outside the shifted entries)
            gCurrentPlayer->y_position = gCurrentPlayer->unkA0 = exit->y;
            gCurrentPlayer->z_position = exit->z + func_global_asm_80612790((s16)gCurrentPlayer->y_rotation + 0x400) * ent_p1;
            gCurrentPlayer->unkEE = gCurrentPlayer->y_rotation;
            break;
        case 1:
            func_global_asm_806F39E8(gCurrentPlayer, ent_shift ? &ent_ex : exit, 0x32);
    }
    gCurrentPlayer->unk6A &= ~0x200;
    PaaD->unk104->unk15F = exit->unk7 <= 0 ? 1 : exit->unk7;
    if (exit->size & 1) {
        D_global_asm_80770DC9 = 1;
    }
}

// ---- Story co-op: door "swirl" (shrink + spin into/out of the world door) for P2 ----
// The map script sets P1's target point (cmd 0x78 -> PaaD->unk22C/230/234) then calls setAction(90, NULL, 0)
// (cmd 0x19), which only ever targets player 0. In co-op the same action is queued for P2 with the same target.
extern s32 D_global_asm_807FD6F0[];
#define COOP_P_SWIRL 1
RECOMP_PATCH u8 setAction(s16 actionIndex, Actor *actor, u8 playerIndex) {
    // Troff 'n' Scoff circle: the script reacts to P2 (roles swapped) but queues its action for player 0 -> give it to P2.
    if (coop_pad_swap && playerIndex == 0 && current_map == MAP_TROFF_N_SCOFF) {
        playerIndex = 1;
    }
    if (character_change_array[playerIndex].action_initiated < actionIndex) {
        character_change_array[playerIndex].action_initiated = actionIndex;
        character_change_array[playerIndex].unk2A0 = actor;
        character_change_array[playerIndex].unk2A8 = D_global_asm_807FD6F0[0];
        character_change_array[playerIndex].unk2AC = D_global_asm_807FD6F0[1];
        character_change_array[playerIndex].unk2B0 = D_global_asm_807FD6F0[2];
        if (actor) {
            character_change_array[playerIndex].unk2A4 = actor->unk54;
        }
#if COOP_P_SWIRL
        if (coop_active_here() && playerIndex == 0 && actionIndex >= 0x40) {
        }
        if ((actionIndex == 90 || actionIndex == 89) && playerIndex == 0 && coop_active_here()
            && character_change_array[1].does_player_exist) {
            PlayerAdditionalActorData *p1 = character_change_array[0].playerPointer->PaaD;
            PlayerAdditionalActorData *p2 = character_change_array[1].playerPointer->PaaD;
            p2->unk22C = p1->unk22C;
            p2->unk230 = p1->unk230;
            p2->unk234 = p1->unk234;
            // World door entry (the swirl target is the door's centre): the Kong who walks in goes a little to the right of it, so that the pair
            // (the other Kong beside him, at his left) is centred on the door. Not for Hott&Stroff.
            // (entering: the Kong who walks in; arriving (action 90): always P1)
            int swirl_who = (actionIndex == 90) ? 0 : coop_door_trig;
            if (swirl_who >= 0 && D_global_asm_807476F4 != 28 && character_change_array[swirl_who].playerPointer != NULL) { // (28: Hott&Stroff door)
                PlayerAdditionalActorData *ep = (swirl_who == 1) ? p2 : p1;
                s16 era = (s16)character_change_array[swirl_who].playerPointer->y_rotation;
                ep->unk22C -= func_global_asm_80612794(era + 0x400) * COOP_ENTRY_CENTER;
                ep->unk234 -= func_global_asm_80612790(era + 0x400) * COOP_ENTRY_CENTER;
            }
            if (character_change_array[1].action_initiated < actionIndex) {
                character_change_array[1].action_initiated = actionIndex;
                character_change_array[1].unk2A0 = actor;
                if (actor) { character_change_array[1].unk2A4 = actor->unk54; }
                character_change_array[1].unk2A8 = D_global_asm_807FD6F0[0];
                character_change_array[1].unk2AC = D_global_asm_807FD6F0[1];
                character_change_array[1].unk2B0 = D_global_asm_807FD6F0[2];
            }
        }
#endif
        return TRUE;
    }
    return FALSE;
}

// Diagnostic switches for the latest co-op patches (1 = enabled, 0 = original game code).
#define COOP_P_GRAB   1
#define COOP_P_PICKUP 1
#define COOP_P_VINE   1
#define COOP_P_COUNT  1
static u8 coop_pickup_player = 0; // player whose pickup test is running (read by changeCollectableCount)
// Player whose HUD must react to a HUD event (item appears, counter changes): the Kong that picks something up, or
// the Kong whose actor is currently being updated.
int coop_hud_owner(void) {
    if (!coop_active_here()) return 0;
    if (coop_pickup_player == 1) return 1;
    return cc_player_index == 1 ? 1 : 0;
}
#if COOP_P_GRAB
// Diagnostic switches for the latest co-op patches (1 = enabled, 0 = original game code).
// ---- Story co-op: grabbable props (vines, trees, ladders) ----
// The original only registers grabs for i < cc_number_of_players (1 in co-op), so P2 never receives
// the "grab" action. In co-op both Kongs are processed.
extern void func_global_asm_8072F230(Actor *, u8, u8);

RECOMP_PATCH void func_global_asm_8072F120(void) {
    u8 i;
    s32 var_v1;
    u8 n = coop_active_here() ? 2 : cc_number_of_players;

    for (i = 0; i < n; i++) {
        // Co-op: this is called from the update of the player whose flag is set; only search for THAT Kong. Searching for
        // both made a Kong already hanging on a tree/vine re-grab (grab action, sounds, pushed down) each time the other jumped.
        if (coop_active_here() && i != cc_player_index) continue;
        if (character_change_array[i].does_player_exist) {
            var_v1 = isFlagSet(PERMFLAG_ITEM_MOVE_VINES, FLAG_TYPE_PERMANENT)
                || (D_global_asm_807FBB64 & 0x10000000)
                || (cc_number_of_players > 1)
                || coop_active_here()
                || gameIsInDKTVMode();
            func_global_asm_8072F230(character_change_array[i].playerPointer, i, var_v1);
        }
    }
}

#endif // COOP_P_GRAB
#if COOP_P_PICKUP
// ---- Story co-op: collectible pickup test (bananas, coins, ammo, medals...) ----
// The original runs the per-player pickup test only for i < cc_number_of_players (1 in co-op), so P2 never
// collects anything. In co-op both Kongs are tested, with cc_player_index set to the tested player (the
// kong-colour check in func_global_asm_806F6644 reads it).
extern u8 D_global_asm_807FD738;
extern void func_global_asm_806F603C(void);
extern u8 func_global_asm_806F6E58(s16);
extern void func_global_asm_806F69A8(u8);
extern s16 D_global_asm_807FD790[];
extern u8 D_global_asm_807FD798;
extern CoopQ36 *D_global_asm_807FD734;

RECOMP_PATCH void func_global_asm_806F6F28(void) {
    u8 sp45;
    s8 cameraShockaveUnlocked;
    u8 playerIndex;
    u8 n;
    u8 saved_cc = cc_player_index;
    PlayerAdditionalActorData *PaaD;
    s16 acc_[4];
    u8 acc_n_ = 0, k_, called_ = 0;

    cameraShockaveUnlocked = FALSE;
    sp45 = FALSE;
    if (D_global_asm_807FBB64 & 0x10000) {
        func_global_asm_806F603C();
    }
    if (isFlagSet(PERMFLAG_ITEM_MOVE_SHOCKWAVE_CAMERA, FLAG_TYPE_PERMANENT)) {
        cameraShockaveUnlocked = TRUE;
    }
    if (func_global_asm_806F6E58(current_character_index[0])) {
        sp45 = TRUE;
    }
    n = coop_active_here() ? 2 : cc_number_of_players;
    for (playerIndex = 0; playerIndex < n; playerIndex++) {
        if (!character_change_array[playerIndex].does_player_exist || character_change_array[playerIndex].playerPointer == NULL) {
            continue;
        }
        PaaD = character_change_array[playerIndex].playerPointer->additional_actor_data;
        if (cameraShockaveUnlocked) {
            PaaD->unk1F4 |= 1;
        } else {
            PaaD->unk1F4 &= ~1;
        }
        if (sp45) {
            PaaD->unk1F4 |= 2;
        } else {
            PaaD->unk1F4 &= ~2;
        }
        if ((character_change_array[playerIndex].playerPointer->control_state != 0x36) && !D_global_asm_807FD738) {
            CoopQ36 *keep_head = NULL, *keep_tail = NULL, *held_head = NULL, *held_tail = NULL, *q, *nx;
            cc_player_index = playerIndex;
            coop_pickup_player = playerIndex;
            // The pending-pickup list (D_global_asm_807FD730: flying "collected" items) is global and func_global_asm_806F69A8
            // advances EVERY item of it on each call and applies the arrival to the calling player. With one call per Kong
            // the animations ran twice as fast and an item was credited to whichever Kong's call finished it. So each call only
            // sees the items its player owns (unk1B), the others are put back afterwards.
            for (q = D_global_asm_807FD730; q != NULL; q = nx) {
                nx = q->next;
                q->next = NULL;
                if ((q->unk1B == 1 ? 1 : 0) == playerIndex) {
                    if (keep_tail) keep_tail->next = q; else keep_head = q;
                    keep_tail = q;
                } else {
                    if (held_tail) held_tail->next = q; else held_head = q;
                    held_tail = q;
                }
            }
            D_global_asm_807FD730 = keep_head;
            D_global_asm_807FD734 = NULL; // force the original code to recompute the tail of the (filtered) list
            func_global_asm_806F69A8(playerIndex);
            // func_global_asm_806F69A8 clears the list of "objects picked up this frame" (D_global_asm_807FD790, read by the script condition 0x22:
            // the Japes golden banana in front of Diddy's cage starts its scene with it) at the start of every call, so P2's call wiped what P1's call
            // had just collected. The ids are accumulated over the calls and put back after the loop.
            called_ = 1;
            for (k_ = 0; k_ < D_global_asm_807FD798 && acc_n_ < 4; k_++) acc_[acc_n_++] = D_global_asm_807FD790[k_];
            if (held_head != NULL) {
                if (D_global_asm_807FD730 == NULL) {
                    D_global_asm_807FD730 = held_head;
                } else {
                    q = D_global_asm_807FD730;
                    while (q->next != NULL) q = q->next;
                    q->next = held_head;
                }
            }
            D_global_asm_807FD734 = NULL;
            coop_pickup_player = 0;
        }
    }
    if (called_) {
        for (k_ = 0; k_ < acc_n_; k_++) D_global_asm_807FD790[k_] = acc_[k_];
        D_global_asm_807FD798 = acc_n_;
    }
    cc_player_index = saved_cc;
}

#endif // COOP_P_PICKUP
#if COOP_P_VINE
// ---- Story co-op: swinging vines/chains (wrappers of func_global_asm_80698120) ----
// The original vine update only looks at P1 (gPlayerPointer / character_change_array[0].playerPointer), so a
// vine grabbed by P2 stayed straight. The original function is kept untouched: when P2 is the one holding this
// vine (P2's grab target PaaD->unk130), P1's pointers are swapped for P2's during the call.
extern void func_global_asm_80698120(u8 arg0);

static void coop_vine_update(u8 n) {
    Actor *saved_g = gPlayerPointer;
    Actor *saved_c = character_change_array[0].playerPointer;
    if (coop_active_here() && character_change_array[1].does_player_exist) {
        Actor *p2 = character_change_array[1].playerPointer;
        if (p2 != NULL && p2->additional_actor_data != NULL
            && ((PlayerAdditionalActorData *)p2->additional_actor_data)->unk130 == (void *)gCurrentActorPointer) {
            gPlayerPointer = p2;
            character_change_array[0].playerPointer = p2;
        }
    }
    func_global_asm_80698120(n);
    gPlayerPointer = saved_g;
    character_change_array[0].playerPointer = saved_c;
}

RECOMP_PATCH void func_global_asm_80698FEC(void) { coop_vine_update(0xE); }
RECOMP_PATCH void func_global_asm_8069900C(void) { coop_vine_update(0xB); }
RECOMP_PATCH void func_global_asm_8069902C(void) { coop_vine_update(0xB); }
RECOMP_PATCH void func_global_asm_8069904C(void) { coop_vine_update(0xE); }
#endif // COOP_P_VINE
#if COOP_P_COUNT
// ---- Story co-op: collectible counters (changeCollectableCount) ----
// The counters are shared (cc_number_of_players == 1, one HUD), but a pickup made by P2 must go to P2's kong
// (e.g. Diddy's red bananas, not Donkey's yellow ones): the HUD counter pointer (into P1's kong progress) is
// moved to the same field of P2's kong.
typedef struct { u16 *actual_count_pointer; u8 pad4[0x2C - 4]; u8 unk_2c; u8 unk_2d; u8 pad2E[2]; } CoopHUDItem; // sizeof 0x30
typedef struct { u8 pad0[0xA]; u16 coloured_bananas[14]; u16 coloured_bananas_fed_to_tns[14]; } CoopCharProgress;
extern CoopHUDItem *func_global_asm_806F7FD0(u8);
extern u16 func_global_asm_806F8EDC(s32, s32);
extern s32 func_global_asm_805FEF10(s32 *);
extern void func_global_asm_80687C48(void);
extern s32 func_global_asm_807319D8(s32, s32, s32);
extern void func_global_asm_806F8278(u8);
extern void playSong(s32, f32);
extern u8 getLevelIndex(u8 map, u8 arg1);

extern int coop_hud_n;
RECOMP_PATCH void changeCollectableCount(s32 HUDItemIndex, u8 playerIndex, s16 amount) {
    CoopHUDItem *HUDItem;
    CoopCharProgress *sp48;
    s32 maxItemCapacity;
    s32 levelIndex;
    s32 flagIndex;
    s32 amountBefore;
    u16 *cnt;
    u8 kong;
    int coop2 = coop_active_here() && coop_hud_n == 2;

    // A pickup made by P2 is reported as player 0 by the deferred pickup queue: it belongs to P2's HUD counters
    // (per-Kong fields point into P1's progress at P2's Kong, pools point into P2's own progress).
    if (coop2 && playerIndex == 0 && coop_pickup_player == 1) {
        playerIndex = 1;
    }
    kong = current_character_index[playerIndex];
    HUDItem = func_global_asm_806F7FD0(playerIndex);
    cnt = HUDItem[HUDItemIndex].actual_count_pointer;
    sp48 = (CoopCharProgress *)&D_global_asm_807FC950[((coop2 ? 0 : playerIndex) * 0x306) + kong * 0x5E];
    if (((HUDItem[HUDItemIndex].unk_2d == 0) || (amount >= 0)) && (HUDItem[HUDItemIndex].unk_2c == 0)) {
        if ((HUDItemIndex != 0xA) && (HUDItemIndex != 0xC)) {
            if (((*cnt) + amount) < 0) {
                *cnt = 0;
            } else {
                *cnt += amount;
                maxItemCapacity = func_global_asm_806F8EDC(HUDItemIndex, playerIndex);
                if (maxItemCapacity < (*cnt)) {
                    *cnt = maxItemCapacity;
                }
                if ((HUDItemIndex == 8) && (func_global_asm_805FEF10(&flagIndex) != 0)) {
                    setFlag(flagIndex, TRUE, FLAG_TYPE_PERMANENT);
                }
                if ((amount > 0) && (HUDItemIndex == 0)) {
                    levelIndex = getLevelIndex(current_map, 1);
                    amountBefore = sp48->coloured_bananas[levelIndex] + sp48->coloured_bananas_fed_to_tns[levelIndex];
                    if ((amountBefore >= 75) && ((amountBefore - amount) < 75)) {
                        setFlag(func_global_asm_807319D8(PERMFLAG_ITEM_MEDAL_JAPES_DK, levelIndex, kong), TRUE, FLAG_TYPE_PERMANENT);
                        func_global_asm_80687C48();
                    }
                    if ((amountBefore == 100) && ((amountBefore - amount) < 100)) {
                        playSong(0xAC, 1.0f);
                    }
                }
            }
        }
        if (HUDItemIndex == 8) {
            func_global_asm_806F8278(playerIndex);
        }
    }
}
#endif // COOP_P_COUNT

// ---- Story co-op: reaction to a pickup (golden banana / key dance, jingles) ----
// The original reads P1 (gPlayerPointer) and the player index of the "current" extra player info, so when P2 picked up a golden
// banana the DANCE was made by P1. Here the Kong that picked the item up reacts.
extern void func_global_asm_80687CC8(void);
extern u8 D_global_asm_80745838;

// Golden banana dance: both Kongs dance (each with the dance of its own Kong); the scene (camera) is the one of the Kong that
// picked the banana up, the other Kong's own cutscene request is ignored.
// TEST (the 3 coconut switches of Japes did not appear after the 1st golden banana): 0 = only the Kong who picked the banana dances (original flow),
// 1 = both dance (co-op feature)
#define COOP_GB_DANCE_BOTH 1
static int coop_dance_owner = -1;
static u32 coop_dance_stamp = 0;
static int coop_dance_started = 0;
static int coop_gb_dance_ok(Actor *pl) {
    if (pl == NULL) return 0;
    if ((pl->y_position - pl->floor) < 100.0f && pl->control_state != 0x63) {
        if (!(pl->unk6A & 4) || !((pl->unkAC - pl->y_position) > 20.0f)) return 1;
    }
    return 0;
}
int coop_cs_skip(Actor *a) {
    int pi = -1, k;
    if (coop_dance_owner < 0 || !coop_active_here()) return 0;
    if ((u32)((u32)object_timer - coop_dance_stamp) > 30) { coop_dance_owner = -1; return 0; }
    for (k = 0; k < 2; k++) {
        if (a != NULL && character_change_array[k].playerPointer == a) pi = k;
    }
    if (pi < 0) return 0;
    if (pi == coop_dance_owner) { coop_dance_started = 1; return 0; }
    return coop_dance_started ? 1 : 0; // the picker's scene is already running: keep it
}
RECOMP_PATCH void func_global_asm_806F79E0(s16 arg0, u8 arg1) {
    s32 var_v0;
    Actor *pl = gPlayerPointer;
    u8 pidx = extra_player_info_pointer->unk1A4;
    if (coop_active_here() && coop_pickup_player < 2 && character_change_array[coop_pickup_player].playerPointer != NULL) {
        pidx = coop_pickup_player;
        pl = character_change_array[pidx].playerPointer;
    }
    switch (arg0) {
        case 0x8E:
            playSong(MUSIC_35_CRYSTAL_COCONUT_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x57:
            playSong(MUSIC_33_MELON_SLICE_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x56:
            playSound(0x147, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x11:
        case 0x8F:
            playSound(0x157, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x98:
            playSound(0x263, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0xDD:
        case 0xDE:
        case 0xDF:
        case 0xE0:
        case 0xE1:
            playSong(MUSIC_69_BLUEPRINT_GET, 1.0f);
            break;
        case 0x90:
            playSong(MUSIC_18_GB_KEY_GET, 1.0f);
            func_global_asm_80687C48();
            if ((pl->y_position - pl->floor) < 100.0f) {
                if (pl->control_state != 0x63) {
                    if (!(pl->unk6A & 4) || !((pl->unkAC - pl->y_position) > 20.0f)) {
                        setAction(0x29, 0, pidx);
                    }
                }
            }
            break;
        case 0x74:
        case 0x288:
            if (coop_active_here() && pidx < 2) { coop_gb_picker = pidx; coop_gb_stamp = (u32)object_timer; }
            playSong(MUSIC_18_GB_KEY_GET, 1.0f);
            if (COOP_GB_DANCE_BOTH && coop_active_here() && pidx < 2 && character_change_array[1 - pidx].does_player_exist
                && character_change_array[0].playerPointer != NULL && character_change_array[1].playerPointer != NULL) {
                // co-op: whoever picks the banana up, P1 is the owner of the scene (its dance runs the cutscene, with P1's camera and heading) and BOTH
                // Kongs are forced into the dance, whatever their state (the banana still counts for the picker). The scene is played at the place of
                // the pickup: the other Kong is brought there (and sent back afterwards if he was far away).
                int oi = 1 - pidx;
                Actor *po = character_change_array[oi].playerPointer;
                f32 gdx = po->x_position - pl->x_position, gdz = po->z_position - pl->z_position;
                coop_gbd_on = 1; coop_gbd_who = 0; coop_gbtp_t = (u32)object_timer; coop_gbtp_quiet = 0; coop_gbd_have = 0;
                coop_gbd_x = pl->x_position; coop_gbd_z = pl->z_position;
                coop_gbd_y = ((pl->y_position - pl->floor) < 100.0f) ? pl->y_position : pl->floor; // picked in mid-air: the scene is on the ground below
                if (gdx * gdx + gdz * gdz > 150.0f * 150.0f) { // far away: he is brought to the picker for the scene, then sent back
                    coop_gbtp_on = 1; coop_gbtp_who = oi; coop_gbtp_t = (u32)object_timer;
                    coop_gbtp_x = po->x_position; coop_gbtp_y = po->y_position; coop_gbtp_z = po->z_position; coop_gbtp_ang = (s16)po->y_rotation;
                }
                coop_dance_owner = 0;
                coop_dance_stamp = (u32)object_timer;
                coop_dance_started = 0;
                setAction(0x29, 0, 0);
                setAction(0x29, 0, 1);
            } else if ((pl->y_position - pl->floor) < 100.0f) {
                if (pl->control_state != 0x63) {
                    if (!(pl->unk6A & 4) || !((pl->unkAC - pl->y_position) > 20.0f)) {
                        setAction(0x29, 0, pidx);
                    }
                }
            }
            break;
        case 0xA:
        case 0xD:
        case 0x16:
        case 0x1E:
        case 0x1F:
        case 0x1CF:
        case 0x1D0:
            playSound(0x2A0, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
        case 0x1C:
        case 0x1D:
        case 0x23:
        case 0x24:
        case 0x27:
            playSong(MUSIC_23_BANANA_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
        case 0x2B:
            break;
        case 0xB7:
            playSong(MUSIC_145_RAINBOW_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0xEC:
        case 0x1D2:
            playSong(MUSIC_32_MINECART_COIN_GET, 1.0 - (D_global_asm_80745838 * 0.3));
            break;
        case 0x13C:
            playSong(MUSIC_18_GB_KEY_GET, 1.0f);
            var_v0 = 0x41;
            if (current_map == MAP_HELM) {
                var_v0 = 0x29;
            }
            setAction(var_v0, 0, pidx);
            break;
        case 0x18D:
            playSong(MUSIC_18_GB_KEY_GET, 1.0f);
            setAction(0x42, 0, pidx);
            if (COOP_GB_DANCE_BOTH && coop_active_here() && pidx < 2) {
                // crown (battle arena, item 0x18D): the other Kong dances too, with the dance of his own Kong
                int oi = 1 - pidx;
                Actor *po = character_change_array[oi].playerPointer;
                if (character_change_array[oi].does_player_exist && po != NULL) { // (no ground / state guard here: crown dance forced)
                    coop_dance_owner = pidx;
                    coop_dance_stamp = (u32)object_timer;
                    coop_dance_started = 0;
                    setAction(0x42, 0, oi);
                }
            }
            func_global_asm_80687CC8();
            break;
        case 0x1D1:
            playSound(0xAE, 0x7FFF, 63.0f, 1.0f, 5, 0);
            break;
    }
}

// ---- Story co-op: warp pads for P2 (func_global_asm_806E2F3C, "Z" handler) ----
// The original only checks the pad under the player (func_global_asm_806E56EC) when cc_number_of_players >= 2.
// In co-op we stay at 1, so P2 never got the pad action; P1 is left on its normal single-player path.
extern s16 D_global_asm_80753354[];
extern s32 func_global_asm_806E56EC(void);
extern GlobalASMStruct35 D_global_asm_807FBB70;

// Co-op rule: if both Kongs stand on the SAME warp pad, neither can teleport (they just crouch): one has to step
// off first. Two Kongs teleporting from the same pad at once broke the animation (never-ending fall, invisible models).
#define COOP_PAD_BLOCK_SHARED 1
static int coop_pad_shared(void) {
#if COOP_PAD_BLOCK_SHARED
    if (coop_active_here() && character_change_array[0].does_player_exist && character_change_array[1].does_player_exist) {
        Actor *me = gCurrentActorPointer;
        Actor *other = character_change_array[cc_player_index ? 0 : 1].playerPointer;
        if (me != NULL && other != NULL && other != me && me->locked_to_pad == 1 && other->locked_to_pad == 1
            && other->unk10C == me->unk10C) {
            return 1;
        }
    }
#endif
    return 0;
}

RECOMP_PATCH void func_global_asm_806E2F3C(void) {
    s32 phi_v1 = 1;
    if (cc_number_of_players >= 2 || (coop_active_here() && cc_player_index == 1)) {
        phi_v1 = coop_pad_shared() ? 1 : !func_global_asm_806E56EC();
    }
    if (phi_v1 != 0) {
        if ((gCurrentActorPointer->control_state != 0x62) && (gCurrentActorPointer->unk6A & 1)) {
            extra_player_info_pointer->unk48 = D_global_asm_80753354[D_global_asm_807FD584];
            gCurrentActorPointer->control_state = 0x62;
            gCurrentActorPointer->control_state_progress = 0;
            playAnimation(gCurrentActorPointer, 0x52);
        }
    }
}

// ---- Story co-op: warp pads / map-object scripts for P2 (func_global_asm_8062BAE4) ----
// Map-object scripts (warp pads...) are written for P1: their conditions read gPlayerPointer /
// character_change_array[0] / current_character_index[0] and their events are sent to P1. While P2 is standing
// on a pad and P1 is not, the roles are swapped for the duration of the script update so that the pad reacts
// to P2 (P2 then gets the pad event and teleports). P1 is unaffected the rest of the time.
extern void func_global_asm_8063B4A4(void);
extern void func_global_asm_8062BB2C(s32, s32, u8);
extern void func_global_asm_8063B758(void);
extern void func_global_asm_80641A78(void);
extern s32 D_global_asm_807F5DE4;
extern s32 D_global_asm_807F5E60;

// Japes, first cave: the door (script object 43) closes again (state 7 -> 8) when the main player is not Diddy and the
// permanent flag 0x1F is not set. In co-op the door must stay open while ONE of the two Kongs is Diddy: the condition
// "not Diddy" (0x8019, arg 3) of that object is switched off (0x8000 = always false) while a Diddy is present.
// TEST (the Diddy / coconut switches scene of Japes is not triggered in co-op): 0 = this rewrite of the script object 43 is switched off
#define COOP_DIDDY_DOOR_RULE 1
static void coop_door_diddy_rule(void) {
    s16 mi;
    s16 oi;
    PropScriptBlock *blk;
    int diddy;
    int i;
    if (!COOP_DIDDY_DOOR_RULE) return;
    if (current_map != MAP_JAPES || !coop_active_here() || !character_change_array[1].does_player_exist) return;
    mi = D_global_asm_807F6240[43];
    if (mi == -1) return;
    oi = func_global_asm_80659470(mi);
    if (oi == -1 || D_global_asm_807F6000[oi].unk7C == NULL) return;
    diddy = (current_character_index[0] == 1 || current_character_index[1] == 1);
    for (blk = D_global_asm_807F6000[oi].unk7C->unkA0; blk != NULL; blk = blk->next) {
        for (i = 0; i < blk->condition_count && i < 5; i++) {
            PropScriptInstruction *c = &blk->conditions[i];
            if ((c->instruction == (s16)0x8019 || c->instruction == (s16)0x8000) && c->args[0] == 3) {
                c->instruction = diddy ? (s16)0x8000 : (s16)0x8019;
            }
        }
    }
}

extern s16 func_global_asm_8063C328(s16 arg0);
// 1 when the Kong stands on a warp pad. Measured object types: pads 0x210 (yellow), 0x211 (purple), 0x212 (red), 0x213 (green), 0x214 (blue);
// palm leaves 0x6C, ship elevator 0x26C, weapon switch 0x128.
static int coop_on_warp_pad(Actor *a) {
    s16 t;
    if (a == NULL || a->locked_to_pad != 1) return 0;
    t = func_global_asm_8063C328((s16)a->unk10C);
    return t >= 0x210 && t <= 0x214;
}

RECOMP_PATCH void func_global_asm_8062BAE4(void) {
    Actor *saved_g = gPlayerPointer;
    Actor *saved_c0 = character_change_array[0].playerPointer;
    u8 saved_k0 = current_character_index[0];
    int swapped = 0;

    coop_door_diddy_rule();
    if (coop_active_here() &&
        character_change_array[1].does_player_exist
        && character_change_array[1].playerPointer != NULL && saved_c0 != NULL) {
        Actor *p2 = character_change_array[1].playerPointer;
        if (coop_mg_ret > 0) coop_mg_ret--;
        if (coop_door_ret1 > 0) coop_door_ret1--;
        // P2 on an object and P1 on none (switches, platforms...), or P2 on a WARP PAD and P1 not on one: locked_to_pad == 1 is also true on palm
        // tree leaves, elevators... (the prop id is in unk10C), so only the type of the object tells a warp pad (0x210..0x214) from the rest.
        int p2_warp = coop_on_warp_pad(p2);
        int p1_warp = coop_on_warp_pad(saved_c0);
        if ((p2->locked_to_pad == 1 && (saved_c0->locked_to_pad != 1 || (p2_warp && !p1_warp))) || coop_mg_ret > 0) {
            gPlayerPointer = p2;
            character_change_array[0].playerPointer = p2;
            current_character_index[0] = current_character_index[1];
            swapped = 1;
        }
    }
    coop_pad_owner = swapped ? 1 : 0;
    coop_pad_swap = swapped;
    func_global_asm_8063B4A4();
    func_global_asm_8062BB2C(D_global_asm_807F5DE4, D_global_asm_807F5E60, 1);
    func_global_asm_8063B758();
    func_global_asm_80641A78();
    coop_pad_swap = 0;
    if (swapped) {
        gPlayerPointer = saved_g;
        character_change_array[0].playerPointer = saved_c0;
        current_character_index[0] = saved_k0;
    }
}

// Script condition 0x13 "a player is within R units of this object". The door objects (world doors 0x2AD, Hott&Stroff doors 0x2AC) use it, in state 1, to
// play their ARRIVAL scene (cutscene 29, swirl-out of the Kong who comes out of the door). In co-op the loop covers both Kongs, so a Kong left standing near
// a door (P2 near the Hott&Stroff door) made that door play its exit scene (camera on the door, swirl sound) when P1 came out of the shop instead.
// During the arrival (first ticks of a map) only P1, who is the one who walks out of every door, is considered.
extern u8 func_global_asm_8064216C(s16 cap, f32 x, f32 y, f32 z);
RECOMP_PATCH s32 func_global_asm_80642204(s16 arg0) {
    u8 playerIndex;
    u8 n = D_global_asm_80750AB4;
    if (coop_active_here() && n > 1 && (u32)object_timer < 300) n = 1;
    for (playerIndex = 0; playerIndex < n; playerIndex++) {
        if ((character_change_array[playerIndex].does_player_exist)
            && (func_global_asm_8064216C(arg0,
                              character_change_array[playerIndex].playerPointer->x_position,
                              character_change_array[playerIndex].playerPointer->y_position,
                              character_change_array[playerIndex].playerPointer->z_position))) {
            return TRUE;
        }
    }
    return FALSE;
}

// Warp pad: the original drops the global geometry budget (D_global_asm_807F9064) to 40000 while a Kong teleports and restores it
// afterwards. The budget is shared by both views, so the other player's screen flickered (reduced scene). Skipped in co-op.
extern u32 D_global_asm_807F9064;
extern u32 D_global_asm_807F9068;
RECOMP_PATCH void func_global_asm_8065EACC(void) {
    if (coop_active_here() && character_change_array[1].does_player_exist) return;
    if (D_global_asm_807F9064 != 40000) {
        D_global_asm_807F9068 = D_global_asm_807F9064;
    }
    D_global_asm_807F9064 = 40000;
}

RECOMP_PATCH void func_global_asm_8065EAF4(void) {
    if (coop_active_here() && character_change_array[1].does_player_exist) return;
    D_global_asm_807F9064 = D_global_asm_807F9068;
}

// ---- Story co-op: warp pad action (func_global_asm_806E4D84) ----
extern f32 D_global_asm_807531E0[];
extern s16 D_global_asm_807534D4[];
extern f32 D_global_asm_807534B8[];

RECOMP_PATCH void func_global_asm_806E4D84(void) {
    if (coop_pad_shared() || !func_global_asm_806E56EC()) {
        if (extra_player_info_pointer->unkC8 == -1) {
            if (current_character_index[cc_player_index] != 6 || gCurrentActorPointer->unkB8 < D_global_asm_807531E0[D_global_asm_807FD584]) {
                gCurrentActorPointer->control_state = 0x3C;
                gCurrentActorPointer->control_state_progress = 0;
                playAnimation(gCurrentActorPointer, 0xA);
                extra_player_info_pointer->unk48 = D_global_asm_807534D4[D_global_asm_807FD584];
            }
            extra_player_info_pointer->unk30 = D_global_asm_807534B8[D_global_asm_807FD584];
        }
    }
}

// ---- Story co-op: camera angle used to turn the stick into a direction (func_global_asm_8061C0FC) ----
// The stick is turned into a world direction with the angle of the player's own camera (unk2C8, see func_global_asm_806E...: unk28 = unk2C8 + stick angle).
// The door / lobby "autowalk" computes its simulated stick for player 0's camera. When P2 is the walker (he walked into the door), his own camera
// has another yaw and he walked sideways (log: P2 left by ~300 units in the swirl-out of the world door, P1 did not). _orig is the original body;
// _coop gives P2 the angle of P1's camera while he is the autowalker. COOP_CAMYAW_USE_ORIG 1 forces the original.
#define COOP_CAMYAW_USE_ORIG 0
#define COOP_TWO_PI 6.28318548202514648
typedef struct {
    u8 unk0[0xC - 0x0];
    f32 unkC;
    u8 unk10[0xA8 - 0x10];
    f32 unkA8;
    s32 unkAC;
    u8 unkB0[0xFB - 0xB0];
    u8 unkFB;
} CoopAAD_8061C0FC;
extern f32 func_global_asm_80665E48(f32 x1, f32 z1, f32 x2, f32 z2);
extern void func_global_asm_8060B55C(s16 *arg0);

static void coop_camyaw_orig(CoopAAD_8061C0FC *arg0) {
    character_change_array[arg0->unkFB].unk2C4 = func_global_asm_80665E48(
        character_change_array[arg0->unkFB].look_at_eye_x,
        character_change_array[arg0->unkFB].look_at_eye_z,
        character_change_array[arg0->unkFB].look_at_at_x,
        character_change_array[arg0->unkFB].look_at_at_z
    );
    arg0->unkA8 = character_change_array[arg0->unkFB].unk2C4;
    switch (is_autowalking) {
        case 0:
            character_change_array[arg0->unkFB].unk2C8 = (arg0->unkA8 * 4096.0) / COOP_TWO_PI;
            break;
        case 2:
            func_global_asm_8060B55C(&character_change_array[arg0->unkFB].unk2C8);
            break;
        case 3:
            character_change_array[arg0->unkFB].unk2C8 = (arg0->unkA8 * 4096.0) / COOP_TWO_PI;
            break;
    }
    if (character_change_array[arg0->unkFB].unk2E9) {
        character_change_array[arg0->unkFB].unk2E9--;
    }
}

static void coop_camyaw_coop(CoopAAD_8061C0FC *arg0) {
    coop_camyaw_orig(arg0);
    // P2 autowalker: the autowalk's stick is for player 0's camera
    if (arg0->unkFB == 1 && is_autowalking == 3 && character_change_array[1].playerPointer != NULL && D_global_asm_807FD710 == character_change_array[1].playerPointer) {
        character_change_array[1].unk2C8 = (character_change_array[0].unk2C4 * 4096.0) / COOP_TWO_PI;
    }
}

RECOMP_PATCH void func_global_asm_8061C0FC(CoopAAD_8061C0FC *arg0) {
#if COOP_CAMYAW_USE_ORIG
    coop_camyaw_orig(arg0);
#else
    if (coop_active_here() && character_change_array[1].does_player_exist) {
        coop_camyaw_coop(arg0);
    } else {
        coop_camyaw_orig(arg0);
    }
#endif
}
// ---- Story co-op: warp pad "teleporting" state (func_global_asm_806DB670, control state 0x52) ----
// The original clears the bits 0x30020 of global_properties_bitfield when the Kong is moved to the destination pad and sets them again 20 frames later.
// They are global (shared by both views): the props (switches, warp pads, palm trees...) of the OTHER Kong's view flickered at that moment.
// _orig is the original body (verbatim); _coop is the same without the two bitfield changes. COOP_WARP_USE_ORIG 1 forces the original, to compare.
#define COOP_WARP_USE_ORIG 0
extern Actor *gLastSpawnedActor;
extern void moveAndScaleActorToAnother(Actor *destination, Actor *source, f32 scale);
extern void func_global_asm_80672C30(Actor *arg0);
extern void coop_warp_806C8220(s32, void *, s32, Actor *) __asm__("func_global_asm_806C8220");
extern void func_global_asm_806291B4(u8 arg0);
extern void func_global_asm_80602498(void);
extern s32 handleInputsForControlState(s32 arg0);
extern void playAnimation(Actor *arg0, s32 arg2);
extern void setYAccelerationFrom80753578(void);
extern void applyActorYAcceleration(void);
extern void func_global_asm_80617770(Actor *arg0, u32 arg1, u8 arg2);
extern void func_global_asm_806C8D20(Actor *arg0);
extern void func_global_asm_806CC970(void);
extern void renderActor(Actor *arg0, u8 arg1);
extern void func_global_asm_8065EACC(void);
extern void func_global_asm_8065EAF4(void);

static void coop_warp_orig(void) {
    gCurrentActorPointer->unkB8 = 0;
    switch (gCurrentActorPointer->control_state_progress) {
        case 0:
            gCurrentActorPointer->noclip_byte = 1;
            if (spawnActor(ACTOR_BANAPORTER, 0x98)) {
                moveAndScaleActorToAnother(gLastSpawnedActor, gCurrentActorPointer, 0.15f);
            }
            gCurrentActorPointer->control_state_progress++;
            break;
        case 1:
            gCurrentActorPointer->control_state_progress++;
            extra_player_info_pointer->unk23C = 4;
            break;
        case 2:
            gCurrentActorPointer->x_position = ((extra_player_info_pointer->unk210 - gCurrentActorPointer->x_position) * 0.5) + gCurrentActorPointer->x_position;
            gCurrentActorPointer->y_position = ((extra_player_info_pointer->unk214 - gCurrentActorPointer->y_position) * 0.5) + gCurrentActorPointer->y_position;
            gCurrentActorPointer->z_position = ((extra_player_info_pointer->unk218 - gCurrentActorPointer->z_position) * 0.5) + gCurrentActorPointer->z_position;
            if (extra_player_info_pointer->unk23C != 0) {
                extra_player_info_pointer->unk23C--;
                if (extra_player_info_pointer->unk23C == 0) {
                    gCurrentActorPointer->object_properties_bitfield &= 0xFFFF7FFF;
                    gCurrentActorPointer->shadow_opacity = 0;
                }
            }
            break;
        case 3:
            gCurrentActorPointer->object_properties_bitfield &= 0xFFFF7FFF;
            gCurrentActorPointer->shadow_opacity = 0;
            gCurrentActorPointer->control_state_progress++;
            break;
        case 5:
            extra_player_info_pointer->unk23C = 0x14;
            break;
        case 6:
            extra_player_info_pointer->unk23C -= 1;
            if (extra_player_info_pointer->unk23C == 0) {
                func_global_asm_8065EACC();
                gCurrentActorPointer->control_state_progress++;
            }
            break;
        case 7:
            global_properties_bitfield &= 0xFFFCFFDF;
            func_global_asm_80672C30(gCurrentActorPointer);
            gCurrentActorPointer->x_position = extra_player_info_pointer->unk204;
            gCurrentActorPointer->y_position = extra_player_info_pointer->unk208;
            gCurrentActorPointer->unk8C = gCurrentActorPointer->y_position;
            gCurrentActorPointer->z_position = extra_player_info_pointer->unk20C;
            coop_warp_806C8220(0, gCurrentActorPointer->unk178, gCurrentActorPointer->unk58, gCurrentActorPointer);
            func_global_asm_8061EB04(gCurrentActorPointer, extra_player_info_pointer->unk1A4);
            extra_player_info_pointer->unk23C = 0x14;
            func_global_asm_806291B4(2);
            gCurrentActorPointer->control_state_progress++;
            break;
        case 8:
            global_properties_bitfield |= 0x30020;
            extra_player_info_pointer->unk23C -= 1;
            if (extra_player_info_pointer->unk23C == 0) {
                func_global_asm_80602498();
                if (spawnActor(ACTOR_BANAPORTER, 0x98)) {
                    moveAndScaleActorToAnother(gLastSpawnedActor, gCurrentActorPointer, 0.15f);
                }
                gCurrentActorPointer->control_state_progress++;
                extra_player_info_pointer->unk23C = 0xC;
            }
            break;
        case 10:
            if (extra_player_info_pointer->unk23C != 0) {
                extra_player_info_pointer->unk23C--;
                if (extra_player_info_pointer->unk23C == 0) {
                    gCurrentActorPointer->x_position = extra_player_info_pointer->unk210;
                    gCurrentActorPointer->y_position = extra_player_info_pointer->unk214;
                    gCurrentActorPointer->z_position = extra_player_info_pointer->unk218;
                    gCurrentActorPointer->object_properties_bitfield |= 0x8000;
                }
            }
            break;
        case 11:
            handleInputsForControlState(0x20);
            playAnimation(gCurrentActorPointer, 0x1B);
            setYAccelerationFrom80753578();
            gCurrentActorPointer->control_state_progress++;
            break;
        case 12:
        case 13:
            handleInputsForControlState(0x20);
            applyActorYAcceleration();
            break;
        case 14:
            func_global_asm_80617770(gCurrentActorPointer, 0x2F, 0);
            gCurrentActorPointer->control_state_progress++;
            // fallthrough
        case 15:
            func_global_asm_806C8D20(gCurrentActorPointer);
            func_global_asm_8065EAF4();
            handleInputsForControlState(0x30);
            break;
    }
    func_global_asm_806CC970();
    renderActor(gCurrentActorPointer, 0);
}

static void coop_warp_coop(void) {
    gCurrentActorPointer->unkB8 = 0;
    switch (gCurrentActorPointer->control_state_progress) {
        case 0:
            gCurrentActorPointer->noclip_byte = 1;
            if (spawnActor(ACTOR_BANAPORTER, 0x98)) {
                moveAndScaleActorToAnother(gLastSpawnedActor, gCurrentActorPointer, 0.15f);
            }
            gCurrentActorPointer->control_state_progress++;
            break;
        case 1:
            gCurrentActorPointer->control_state_progress++;
            extra_player_info_pointer->unk23C = 4;
            break;
        case 2:
            gCurrentActorPointer->x_position = ((extra_player_info_pointer->unk210 - gCurrentActorPointer->x_position) * 0.5) + gCurrentActorPointer->x_position;
            gCurrentActorPointer->y_position = ((extra_player_info_pointer->unk214 - gCurrentActorPointer->y_position) * 0.5) + gCurrentActorPointer->y_position;
            gCurrentActorPointer->z_position = ((extra_player_info_pointer->unk218 - gCurrentActorPointer->z_position) * 0.5) + gCurrentActorPointer->z_position;
            if (extra_player_info_pointer->unk23C != 0) {
                extra_player_info_pointer->unk23C--;
                if (extra_player_info_pointer->unk23C == 0) {
                    gCurrentActorPointer->object_properties_bitfield &= 0xFFFF7FFF;
                    gCurrentActorPointer->shadow_opacity = 0;
                }
            }
            break;
        case 3:
            gCurrentActorPointer->object_properties_bitfield &= 0xFFFF7FFF;
            gCurrentActorPointer->shadow_opacity = 0;
            gCurrentActorPointer->control_state_progress++;
            break;
        case 5:
            extra_player_info_pointer->unk23C = 0x14;
            break;
        case 6:
            extra_player_info_pointer->unk23C -= 1;
            if (extra_player_info_pointer->unk23C == 0) {
                func_global_asm_8065EACC();
                gCurrentActorPointer->control_state_progress++;
            }
            break;
        case 7:
            // (co-op) global_properties_bitfield &= 0xFFFCFFDF is not done: it is shared by both views
            func_global_asm_80672C30(gCurrentActorPointer);
            gCurrentActorPointer->x_position = extra_player_info_pointer->unk204;
            gCurrentActorPointer->y_position = extra_player_info_pointer->unk208;
            gCurrentActorPointer->unk8C = gCurrentActorPointer->y_position;
            gCurrentActorPointer->z_position = extra_player_info_pointer->unk20C;
            coop_warp_806C8220(0, gCurrentActorPointer->unk178, gCurrentActorPointer->unk58, gCurrentActorPointer);
            func_global_asm_8061EB04(gCurrentActorPointer, extra_player_info_pointer->unk1A4);
            extra_player_info_pointer->unk23C = 0x14;
            func_global_asm_806291B4(2);
            gCurrentActorPointer->control_state_progress++;
            break;
        case 8:
            // (co-op) global_properties_bitfield |= 0x30020 is not done either
            extra_player_info_pointer->unk23C -= 1;
            if (extra_player_info_pointer->unk23C == 0) {
                func_global_asm_80602498();
                if (spawnActor(ACTOR_BANAPORTER, 0x98)) {
                    moveAndScaleActorToAnother(gLastSpawnedActor, gCurrentActorPointer, 0.15f);
                }
                gCurrentActorPointer->control_state_progress++;
                extra_player_info_pointer->unk23C = 0xC;
            }
            break;
        case 10:
            if (extra_player_info_pointer->unk23C != 0) {
                extra_player_info_pointer->unk23C--;
                if (extra_player_info_pointer->unk23C == 0) {
                    gCurrentActorPointer->x_position = extra_player_info_pointer->unk210;
                    gCurrentActorPointer->y_position = extra_player_info_pointer->unk214;
                    gCurrentActorPointer->z_position = extra_player_info_pointer->unk218;
                    gCurrentActorPointer->object_properties_bitfield |= 0x8000;
                }
            }
            break;
        case 11:
            handleInputsForControlState(0x20);
            playAnimation(gCurrentActorPointer, 0x1B);
            setYAccelerationFrom80753578();
            gCurrentActorPointer->control_state_progress++;
            break;
        case 12:
        case 13:
            handleInputsForControlState(0x20);
            applyActorYAcceleration();
            break;
        case 14:
            func_global_asm_80617770(gCurrentActorPointer, 0x2F, 0);
            gCurrentActorPointer->control_state_progress++;
            // fallthrough
        case 15:
            func_global_asm_806C8D20(gCurrentActorPointer);
            func_global_asm_8065EAF4();
            handleInputsForControlState(0x30);
            break;
    }
    func_global_asm_806CC970();
    renderActor(gCurrentActorPointer, 0);
}

RECOMP_PATCH void func_global_asm_806DB670(void) {
#if COOP_WARP_USE_ORIG
    coop_warp_orig();
#else
    if (coop_active_here() && character_change_array[1].does_player_exist) {
        coop_warp_coop();
    } else {
        coop_warp_orig();
    }
#endif
}
// ---- Story co-op: warp-pad "banaporter" effect (func_global_asm_8069B3FC) ----
// The original drives gPlayerPointer (P1) only: it feeds P1's position targets and advances P1's
// control_state_progress. Here it follows the player that is actually teleporting (control_state 0x52).
typedef struct {
    s16 unk0; // y rotation
} AAD_8069B3FC;

extern void getBonePosition(Actor *, s32, f32 *, f32 *, f32 *);
extern void playActorAnimation(Actor *, s32);
extern void playSong(s32, f32);
extern void func_global_asm_8071498C(void *);
extern void func_global_asm_80717D4C(void);
extern void func_global_asm_807149B8(s32);
extern void func_global_asm_80714950(s32);
extern void func_global_asm_80714C08(void *, f32, Actor *, s32, s32);
extern void deleteActor(Actor *);
extern u8 D_global_asm_8071FC58[];
extern u8 D_global_asm_8071FC8C[];
extern u8 D_global_asm_8071FCC0[];

RECOMP_PATCH void func_global_asm_8069B3FC(void) {
    AAD_8069B3FC *aaD;
    s32 j, i;
    f32 x, y, z;
    PlayerAdditionalActorData *PaaD;
    void *sprite;
    Actor *pl = gPlayerPointer;

    if (coop_active_here() && character_change_array[1].does_player_exist && character_change_array[1].playerPointer != NULL) {
        Actor *p1 = character_change_array[0].playerPointer;
        Actor *p2 = character_change_array[1].playerPointer;
        int s1 = (p1 != NULL && p1->control_state == 0x52);
        int s2 = (p2->control_state == 0x52);
        if (s2 && !s1) {
            pl = p2;
        } else if (s1 && s2) {
            f32 d1 = (p1->x_position - gCurrentActorPointer->x_position) * (p1->x_position - gCurrentActorPointer->x_position)
                   + (p1->z_position - gCurrentActorPointer->z_position) * (p1->z_position - gCurrentActorPointer->z_position);
            f32 d2 = (p2->x_position - gCurrentActorPointer->x_position) * (p2->x_position - gCurrentActorPointer->x_position)
                   + (p2->z_position - gCurrentActorPointer->z_position) * (p2->z_position - gCurrentActorPointer->z_position);
            if (d2 < d1) {
                pl = p2;
            }
        }
    }

    aaD = gCurrentActorPointer->additional_actor_data;
    PaaD = pl->PaaD;
    if (!(gCurrentActorPointer->object_properties_bitfield & 0x10)) {
        gCurrentActorPointer->noclip_byte = 1;
        gCurrentActorPointer->object_properties_bitfield |= 0x400;
        playActorAnimation(gCurrentActorPointer, 0x298);
        aaD->unk0 = 0;
        playSong(59, 1.0f);
    } else {
        getBonePosition(gCurrentActorPointer, 1, &x, &y, &z);
        PaaD->unk210 = x;
        PaaD->unk214 = y;
        PaaD->unk218 = z;
        if (gCurrentActorPointer->control_state_progress < 4) {
            for (i = 2; i < 3; i++) {
                for (j = 0; j < 2; j++) {
                    func_global_asm_8071498C(func_global_asm_80717D4C);
                    func_global_asm_807149B8(1);
                    func_global_asm_80714950((RandClamp(10000) % 120) + 0x50);
                    switch (RandClamp(10000) % 3) {
                        case 0:
                            sprite = &D_global_asm_8071FC58;
                            break;
                        case 1:
                            sprite = &D_global_asm_8071FC8C;
                            break;
                        default:
                            sprite = &D_global_asm_8071FCC0;
                            break;
                    }
                    func_global_asm_80714C08(sprite, 0.6f, gCurrentActorPointer, i, 0);
                }
            }
        }
        switch (gCurrentActorPointer->control_state_progress) {
            case 0:
                aaD->unk0 += 0x1E;
                break;
            case 1:
                pl->control_state_progress++;
                gCurrentActorPointer->control_state_progress++;
                break;
            case 2:
                aaD->unk0 -= 0x2A;
                if (aaD->unk0 < 0) {
                    aaD->unk0 = 0;
                }
                break;
            case 3:
                aaD->unk0 -= 0x2A;
                if (aaD->unk0 < 0) {
                    aaD->unk0 = 0;
                }
                pl->control_state_progress++;
                gCurrentActorPointer->object_properties_bitfield &= 0xFFFF7FFF;
                gCurrentActorPointer->shadow_opacity = 0xFF;
                gCurrentActorPointer->control_state_progress++;
                break;
            case 4:
                aaD->unk0 -= 0x2A;
                if (aaD->unk0 < 0) {
                    aaD->unk0 = 0;
                }
                gCurrentActorPointer->shadow_opacity -= 0x28;
                if (gCurrentActorPointer->shadow_opacity < 0x28) {
                    pl->control_state_progress++;
                    func_global_asm_80602B60(0x3B, 0);
                    deleteActor(gCurrentActorPointer);
                }
                break;
        }
    }
    gCurrentActorPointer->y_rotation += aaD->unk0;
    renderActor(gCurrentActorPointer, 0);
}

// ---- Story co-op: Kong tag barrel ----
// Same as the original (func_global_asm_8068412C) except that the two asm helpers are called in the context of the Kong
// who entered the barrel (cc_player_index / gCurrentPlayer / controller), and the camera is repaired afterwards:
//  - enter (806833DC): player context = the Kong that touched the barrel (D_global_asm_807FBB70.unk1FC);
//  - selection (806836D0): reads controller 0 only -> controller data of P1 and of the user are swapped during the call;
//    when it ends it re-inits the camera of the user with player index 0 (8061EB04(user, 0)), which broke P2's camera
//    and left P1's one in cutscene mode: both cameras are re-initialised with the right index.
// During the selection the scene is drawn on the whole screen (P1's viewport enlarged, P2's skipped, see
// func_global_asm_8062BF24) and the other Kong waits next to the one who is choosing.
extern void func_global_asm_806833DC(YetAnotherAdditionalActorData *);
extern void func_global_asm_806836D0(YetAnotherAdditionalActorData *);
extern void func_global_asm_80683AD8(void);
extern void func_global_asm_80714998(u8);
extern void func_global_asm_807149FC(s32);
extern void func_global_asm_807149A8(s16);
extern void func_global_asm_80714A28(u16);
extern void *drawSpriteAtPosition(void *, f32, f32, f32, f32);
extern s32 func_global_asm_80689F2C(void);
extern void func_global_asm_80665564(Actor *, f32);
extern void func_global_asm_8068408C(Actor *);
extern void func_global_asm_80680774(void);
extern s32 func_global_asm_806C9D7C(void);
extern s32 func_global_asm_806832F4(s32, s32);
extern void func_global_asm_8067AAB8(Actor *, Actor *, s16, f32, s16);
extern void func_global_asm_80679200(Actor *, Actor *, s32, s32, s32, void *);
extern void func_global_asm_806FB2B8(void);
extern void addActorToTextOverlayRenderArray(void *, Actor *, u8);
typedef struct { u8 b[0x34]; } CoopBarrelInput;
extern CoopBarrelInput D_global_asm_807FD610[];

static int coop_barrel_player_index(Actor *a) {
    int k;
    for (k = 0; k < 2; k++) {
        if (character_change_array[k].does_player_exist && character_change_array[k].playerPointer == a) return k;
    }
    return -1;
}

static void coop_barrel_call(void (*fn)(YetAnotherAdditionalActorData *), YetAnotherAdditionalActorData *YaaD, Actor *user, int swap_input) {
    int uidx = coop_active_here() ? coop_barrel_player_index(user) : -1;
    if (uidx <= 0) {
        fn(YaaD); // P1 (or no co-op): original behaviour
        return;
    }
    {
        Actor *saved = character_change_array[cc_player_index].playerPointer;
        CoopBarrelInput tmp;
        func_global_asm_8067B100(user);
        if (swap_input) {
            tmp = D_global_asm_807FD610[0];
            D_global_asm_807FD610[0] = D_global_asm_807FD610[uidx];
        }
        fn(YaaD);
        if (swap_input) {
            D_global_asm_807FD610[uidx] = D_global_asm_807FD610[0];
            D_global_asm_807FD610[0] = tmp;
        }
        if (saved != NULL) func_global_asm_8067B100(saved);
    }
}

// Tag barrel
RECOMP_PATCH void func_global_asm_8068412C(void) {
    YetAnotherAdditionalActorData *YaaD;
    u8 prev_state;
    Actor *user;

    YaaD = gCurrentActorPointer->YaaD;
    if (ACTOR_UNINITIALIZED(gCurrentActorPointer)) {
        YaaD->unk22 = gCurrentActorPointer->y_rotation;
        YaaD->unk24 = gCurrentActorPointer->y_position;
        gCurrentActorPointer->unk146 = 0;
        gCurrentActorPointer->unk15E = 0;
        func_global_asm_80714998(2);
        func_global_asm_807149FC(-1);
        func_global_asm_807149B8(1);
        func_global_asm_807149A8(1000);
        func_global_asm_80714A28(4);
        drawSpriteAtPosition(
            &D_global_asm_8071FC8C,
            1.3f,
            gCurrentActorPointer->x_position,
            gCurrentActorPointer->y_position + 13.0,
            gCurrentActorPointer->z_position
        );
        if (func_global_asm_80689F2C()) {
            gCurrentActorPointer->draw_distance = 2000;
        } else {
            gCurrentActorPointer->draw_distance = 350;
        }
        gCurrentActorPointer->floor = gCurrentActorPointer->y_position;
        func_global_asm_80665564(gCurrentActorPointer, 0.0f);
        gCurrentActorPointer->locked_to_pad = 0;
        gCurrentActorPointer->object_properties_bitfield |= 2;
    }
    func_global_asm_8068408C(gCurrentActorPointer);
    func_global_asm_80680774();
    func_global_asm_8067ACB4(gCurrentActorPointer);

    prev_state = gCurrentActorPointer->control_state;
    switch (gCurrentActorPointer->control_state) {
        case 0:
            if (YaaD->unk4) {
                YaaD->unk4--;
                if (!YaaD->unk4) {
                    gCurrentActorPointer->noclip_byte = 2;
                }
            } else {
                user = (D_global_asm_807FBB70.unk200 == 4) ? D_global_asm_807FBB70.unk1FC : NULL;
                coop_barrel_call(func_global_asm_806833DC, YaaD, user, 0);
            }
            if (gCurrentActorPointer->control_state_progress == 0xA) {
                if ((gPlayerPointer->unk6A & 4) || func_global_asm_806C9D7C()) {
                    gCurrentActorPointer->control_state_progress = 0;
                } else {
                    if ((gPlayerPointer->locked_to_pad != 1) && (gPlayerPointer->unk6A & gPlayerPointer->unk6C & 1)) {
                        func_global_asm_806832F4(gPlayerPointer->unk58, 1);
                    }
                }
            }
            break;
        case 1:
            user = YaaD->unk0;
            coop_barrel_call(func_global_asm_806836D0, YaaD, user, 1);
            addActorToTextOverlayRenderArray(func_global_asm_80683AD8, gCurrentActorPointer, 3);
            break;
        case 2:
            if (YaaD->unk4) {
                YaaD->unk4--;
            } else {
                func_global_asm_8067AAB8(gCurrentActorPointer, YaaD->unk0, YaaD->unk38, 180.0f, 0x46);
                func_global_asm_80679200(YaaD->unk0, gCurrentActorPointer, 0x40, 1, 0, 0);
                func_global_asm_806FB2B8();
                gCurrentActorPointer->control_state = 0;
                gCurrentActorPointer->control_state_progress = 0;
                if (func_global_asm_806832F4(gPlayerPointer->unk58, 0)) {
                    gCurrentActorPointer->control_state_progress = 0xA;
                }
                YaaD->unk4 = 0x1E;
            }
            break;
    }
    if (coop_active_here()) {
        // selection scene running: for P2 its screen shows camera 0 (see func_global_asm_8062BF24)
        if (prev_state != 1 && gCurrentActorPointer->control_state == 1) {
            coop_barrel_user_actor = YaaD->unk0;
        }
        // selection (state 1) + exit wait (state 2) + a grace period while the cutscene finishes
        if (gCurrentActorPointer->control_state != 0) coop_barrel_sel = 120;
        else if (coop_barrel_sel > 0) coop_barrel_sel--;
        coop_barrel_busy = (gCurrentActorPointer->control_state == 1 && coop_barrel_player_index(YaaD->unk0) == 1);
        if (gCurrentActorPointer->control_state == 1) {
            coop_barrel_chooser = coop_barrel_player_index(YaaD->unk0);
            coop_barrel_spot = (Actor *)YaaD->unk1C;
            coop_barrel_stamp = object_timer;
        } else {
            coop_barrel_chooser = -1;
            coop_barrel_spot = NULL;
        }
        if (prev_state == 1 && gCurrentActorPointer->control_state != 1) {
            // selection finished: repair the cameras (the original re-inits the user's camera with index 0)
            int k;
            int uidx = coop_barrel_player_index(YaaD->unk0);
            if (uidx > 0) { // P2 chose: P2's camera (index 0 used by the original) and P1's (left in cutscene mode) are re-initialised
                for (k = 0; k < 2; k++) {
                    if (character_change_array[k].does_player_exist && character_change_array[k].playerPointer != NULL) {
                        func_global_asm_8061EB04(character_change_array[k].playerPointer, k);
                        func_global_asm_80709464(k);
                    }
                }
                character_change_array[uidx].unk2DC.unk6 |= 0x11;
            }
            coop_barrel_user_actor = NULL;
        }
    }
    func_global_asm_8065D254(gCurrentActorPointer, 0x3C8, 0x40, 0x40, 0x1E, 0x1E, 1, 0x64, 0x78, 0, 1.0f);
    if (gCurrentActorPointer->control_state != 1) {
        gCurrentActorPointer->y_rotation += 0x32;
        gCurrentActorPointer->y_rotation &= 0xFFF;
        gCurrentActorPointer->y_position = func_global_asm_80612794(YaaD->unk28) * 5.0f + YaaD->unk24;
        YaaD->unk28 += 0x32;
        YaaD->unk28 &= 0xFFF;
    }
    renderActor(gCurrentActorPointer, 0);
}

// ---- Story co-op: positional sounds (2nd function, camera 0 only in the original) ----
// Same as the original, but in co-op uses the nearest of the two Kong cameras.
extern void *func_global_asm_80737638(void *, s16, u16, u8, f32, u8, void *);
extern void func_global_asm_80737924(void *);
extern void *D_global_asm_807457C8;
extern u8 D_global_asm_80745838;
extern void *D_global_asm_8076D1F8;
extern void *D_global_asm_8076D1FC;
extern u8 D_global_asm_8077058E;
extern f32 D_global_asm_80770DE4;
extern f32 func_global_asm_80665E48(f32, f32, f32, f32);
extern f32 func_global_asm_80612D1C(f32);

RECOMP_PATCH void func_global_asm_8060956C(f32 arg0, f32 arg1, f32 arg2, s16 arg3, u8 arg4, f32 arg5, u8 arg6, u8 arg7) {
    f32 temp_f2;
    s32 var_v1;
    f32 temp_f14;
    u8 sp43;
    f64 var_f6;
    void *temp;
    int cam = 0;

    if (coop_active_here() && character_change_array[1].does_player_exist) {
        f32 d0 = SQ(character_change_array[0].look_at_eye_x - arg0) + SQ(character_change_array[0].look_at_eye_y - arg1) + SQ(character_change_array[0].look_at_eye_z - arg2);
        f32 d1 = SQ(character_change_array[1].look_at_eye_x - arg0) + SQ(character_change_array[1].look_at_eye_y - arg1) + SQ(character_change_array[1].look_at_eye_z - arg2);
        if (d1 < d0) {
            cam = 1;
        }
    }

    sp43 = 0;
    D_global_asm_80770DE4 = _sqrtf(
        SQ((s32)(character_change_array[cam].look_at_eye_x - arg0)) +
        SQ((s32)(character_change_array[cam].look_at_eye_y - arg1)) +
        SQ((s32)(character_change_array[cam].look_at_eye_z - arg2)));
    if (D_global_asm_80770DE4 < 1.0f) {
        D_global_asm_80770DE4 = 1.0f;
    }
    temp_f2 = arg7 * 4000;
    var_v1 = ((temp_f2 / _sqrtf(D_global_asm_80770DE4)) - ((temp_f2 / 7.745966) - 32767.0));
    if (var_v1 >= 0x8000) {
        var_v1 = 0x7FFF;
    }
    var_v1 = (arg4 * var_v1) / 255.0;
    if (var_v1 >= 0x8000) {
        var_v1 = 0x7FFF;
    }
    if (var_v1 >= 0x3E9) {
        var_v1 = (arg4 * var_v1) / 255.0;
        if (arg6 != 0) {
            var_f6 = func_global_asm_806119A0() / (f64)0xFFFFFFFF;
            var_f6 *= (arg6 / 155.0);
            var_v1 = (1.0 - var_f6) * var_v1;
            arg5 = ((func_global_asm_806119A0() / (f64)0xFFFFFFFF) * (arg6 / 100.0)) + (arg5 - (arg6 / 255.0));
        }
        temp_f14 = func_global_asm_80665E48(character_change_array[cam].look_at_eye_x, character_change_array[cam].look_at_eye_z, arg0, arg2) - character_change_array[cam].unk2C4;
        if (temp_f14 < 0.0f) {
            temp_f14 += MATH_2PI_F;
        }
        if ((0.25f * MATH_PI_F) < temp_f14) {
            if (temp_f14 < (1.75 * MATH_PI_F)) {
                var_v1 = ((func_global_asm_80612D1C(((2.0 * temp_f14) / 3.0) + 2.617993950843811) * 0.3) + 1.0) * var_v1;
            }
        }
        temp_f14 = (1.0f - func_global_asm_80612D1C(temp_f14)) * 63.5;
        if (D_global_asm_80745838) {
            sp43 = 0x1E;
        } else if (D_global_asm_8077058E >= 0x15) {
            sp43 = (D_global_asm_8077058E * 100) / 255;
        }
        temp = D_global_asm_807457C8;
        if (temp) {
            func_global_asm_80737924(temp);
        }
        if (arg3 < 0x800) {
            func_global_asm_80737638(D_global_asm_8076D1F8, arg3, var_v1, temp_f14, arg5, sp43, &D_global_asm_807457C8);
        } else {
            func_global_asm_80737638(D_global_asm_8076D1FC, arg3 - 0x7FF, var_v1, temp_f14, arg5, sp43, &D_global_asm_807457C8);
        }
    }
}

// ---- Story co-op: actor/ambient looping sounds (wing flaps etc.) ----
// func_global_asm_80602E6C (asm) computes volume/pan from camera 0 only. We temporarily put the
// nearest Kong's camera data in slot 0 while it runs, then restore.
extern f32 func_global_asm_80602E6C(f32, f32, f32, u8, u8, f32);
extern f32 D_global_asm_80770DDC;
extern f32 D_global_asm_80770DE0;
extern f32 D_global_asm_80770DE8;
extern f32 D_global_asm_80770DEC;
extern f32 D_global_asm_80770DF0;
extern f32 D_global_asm_80770DF4;

RECOMP_PATCH f32 func_global_asm_80603340(f32 arg0, f32 arg1, f32 arg2, u8 arg3, u8 arg4, f32 arg5) {
    f32 temp_f0;
    f32 temp_f2;
    f32 s[7];
    int swapped = 0;

    if (coop_active_here() && character_change_array[1].does_player_exist) {
        f32 d0 = SQ(character_change_array[0].look_at_eye_x - arg0) + SQ(character_change_array[0].look_at_eye_y - arg1) + SQ(character_change_array[0].look_at_eye_z - arg2);
        f32 d1 = SQ(character_change_array[1].look_at_eye_x - arg0) + SQ(character_change_array[1].look_at_eye_y - arg1) + SQ(character_change_array[1].look_at_eye_z - arg2);
        if (d1 < d0) {
            swapped = 1;
            s[0] = character_change_array[0].look_at_eye_x; s[1] = character_change_array[0].look_at_eye_y; s[2] = character_change_array[0].look_at_eye_z;
            s[3] = character_change_array[0].look_at_at_x;  s[4] = character_change_array[0].look_at_at_y;  s[5] = character_change_array[0].look_at_at_z;
            s[6] = character_change_array[0].unk2C4;
            character_change_array[0].look_at_eye_x = character_change_array[1].look_at_eye_x;
            character_change_array[0].look_at_eye_y = character_change_array[1].look_at_eye_y;
            character_change_array[0].look_at_eye_z = character_change_array[1].look_at_eye_z;
            character_change_array[0].look_at_at_x = character_change_array[1].look_at_at_x;
            character_change_array[0].look_at_at_y = character_change_array[1].look_at_at_y;
            character_change_array[0].look_at_at_z = character_change_array[1].look_at_at_z;
            character_change_array[0].unk2C4 = character_change_array[1].unk2C4;
        }
    }

    temp_f0 = func_global_asm_80602E6C(arg0, arg1, arg2, arg3, arg4, arg5);

    if (swapped) {
        character_change_array[0].look_at_eye_x = s[0]; character_change_array[0].look_at_eye_y = s[1]; character_change_array[0].look_at_eye_z = s[2];
        character_change_array[0].look_at_at_x = s[3];  character_change_array[0].look_at_at_y = s[4];  character_change_array[0].look_at_at_z = s[5];
        character_change_array[0].unk2C4 = s[6];
    }

    if (temp_f0 < 10.0f) {
        return 0.0f;
    }
    temp_f2 = D_global_asm_80770DDC * 0.007874015748031496;
    D_global_asm_80770DE8 += temp_f0 * temp_f2;
    D_global_asm_80770DEC += temp_f0 * (1.0f - temp_f2);
    if (D_global_asm_80770DE0 > 0.0f) {
        D_global_asm_80770DF0 += temp_f0 * D_global_asm_80770DE0;
    }
    if (D_global_asm_80770DE0 < 0.0f) {
        D_global_asm_80770DF4 -= temp_f0 * D_global_asm_80770DE0;
    }
    return temp_f0;
}

// ---- Story co-op: trigger zones (cave entrances, etc.) must also react to P2 ----
typedef struct { s16 type; u16 args[4]; } CoopTrigSub;
typedef struct {
    s16 x; s16 y; s16 z; s16 radius; s16 height; s16 unkA;
    u8 activation_type; u8 default_active_state; u8 unkE; u8 unkF;
    CoopTrigSub data[4];
} CoopTrigRaw;
typedef struct { CoopTrigRaw raw; u8 not_in_zone; u8 active; } CoopTrigData;

extern s16 D_global_asm_8076A0AA;
extern s32 func_global_asm_807318AC(s32, s32);

RECOMP_PATCH u8 func_global_asm_8072EA90(CoopTrigData *arg0) {
    u8 var_t3 = FALSE;
    u8 var_t4 = 0;
    u8 playerIndex;
    u8 n = coop_active_here() ? 2 : cc_number_of_players;
    f32 d;

    for (playerIndex = 0; playerIndex < n; playerIndex++) {
        if (character_change_array[playerIndex].does_player_exist != 0 && character_change_array[playerIndex].playerPointer != NULL) {
            Actor *pp = character_change_array[playerIndex].playerPointer;
            if (arg0->raw.height == -1 || (arg0->raw.y <= pp->y_position && pp->y_position < (arg0->raw.y + arg0->raw.height))) {
                d = ((arg0->raw.x - pp->x_position) * (arg0->raw.x - pp->x_position)) + ((arg0->raw.z - pp->z_position) * (arg0->raw.z - pp->z_position));
                if (d < (arg0->raw.radius * arg0->raw.radius)) {
                    var_t3 = TRUE;
                }
            }
        }
    }
    switch (arg0->raw.activation_type) {
        case 0:
            if (var_t3 && arg0->not_in_zone == 1 && arg0->active != 0) { var_t4 = 1; }
            break;
        case 1:
            if (!var_t3 && arg0->not_in_zone == 0 && arg0->active != 0) { var_t4 = 1; }
            break;
        case 2:
            if (var_t3 && arg0->active != 0) { var_t4 = 1; }
            break;
    }
    if (var_t4 != 0) {
        if (func_global_asm_807318AC(D_global_asm_8076A0AA, arg0->raw.unkA) == 0) {
            var_t4 = 0;
        }
    }
    arg0->not_in_zone = var_t3 ? 0 : 1;
    return var_t4;
}

// ---- Story co-op: "no skybox" background (tunnels/temples in Aztec...) ----
// The original fills the WHOLE screen with black, then restores the scissor to camera 0's viewport. Called while drawing
// P2's viewport it wiped P1's already drawn half (and left a wrong scissor). In co-op: fill/restore only the viewport of
// the camera being drawn.
extern s16 D_global_asm_80744498;
extern s16 D_global_asm_8074449C;
extern s16 D_global_asm_807444A0;
extern s16 D_global_asm_807444A4;

RECOMP_PATCH Gfx *func_global_asm_8070770C(Gfx *dl) {
    int cam = 0;
    s16 x0 = D_global_asm_80744498, y0 = D_global_asm_8074449C, x1 = D_global_asm_807444A0, y1 = D_global_asm_807444A4;

    if (coop_active_here() && cc_player_index >= 0 && cc_player_index < 2) {
        cam = cc_player_index;
        x0 = character_change_array[cam].unk270[0];
        y0 = character_change_array[cam].unk270[1];
        x1 = character_change_array[cam].unk270[2];
        y1 = character_change_array[cam].unk270[3];
    }
    gDPPipeSync(dl++);
    gDPSetRenderMode(dl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(dl++, 0x00010001);
    gDPSetCycleType(dl++, G_CYC_FILL);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE, x0, y0, x1, y1);
    gDPFillRectangle(dl++, x0, y0, x1 - 1, y1 - 1);
    gDPPipeSync(dl++);
    gDPSetScissor(dl++, G_SC_NON_INTERLACE,
        character_change_array[cam].unk270[0],
        character_change_array[cam].unk270[1],
        character_change_array[cam].unk270[2],
        character_change_array[cam].unk270[3]);
    gDPSetCycleType(dl++, G_CYC_1CYCLE);
    return dl;
}


// ---- Troff 'n' Scoff hippo (feeding), co-op ----
// The original reads gPlayerPointer for the Kong that feeds (camera, facing the hippo, bananas flying to the Kong). In
// co-op gPlayerPointer can be P2 while P1 is the one feeding (log: effect target = P2), so P2 was turned towards the
// hippo and P1 kept its own direction. Use the Kong that is in the feeding state (0x6C) instead.
extern u16 D_global_asm_807FC930[];
extern u16 D_global_asm_807446C0[];
typedef struct { u8 pad0[0x28]; s32 unk28; s16 unk2C; s16 unk2E; f32 unk30; } CoopStruct807FDC90;
extern CoopStruct807FDC90 *D_global_asm_807FDC90;
extern u8 func_global_asm_806BDD90(void);
extern u16 func_global_asm_806F8AD4(u8, u8);
extern void func_global_asm_8061C518(Actor *, Actor *, u8, s16, s16, s16, s16, s16, s16, s16, f32);
extern void func_global_asm_80724E48(u8);
extern void func_global_asm_807248B0(Actor *, f32);
extern void func_global_asm_80685520(f32, u8, f32, f32, f32);
extern void func_global_asm_8071D0F0(void *, void *);
extern void *func_global_asm_806FACE8(u32);
// CharacterProgress (stride 0x5E): coloured_bananas_fed_to_tns[14] at +0x26
#define COOP_FED_TNS(k) ((u16 *)(D_global_asm_807FC950 + (k) * 0x5E + 0x26))
extern void coop_hud_hide_item(int item, int player);
RECOMP_PATCH void func_global_asm_806BE09C(void) {
    PlayerAdditionalActorData *sp4C;
    u8 levelIndex;
    f32 sp44;
    f32 temp_f2;
    u16 *sp3C;
    s32 i;
    s32 count = 0;
    Actor *pl = gPlayerPointer;
    int f = 0; // player (0/1) who feeds

    if (coop_active_here()) {
        Actor *k0 = character_change_array[0].playerPointer;
        Actor *k1 = character_change_array[1].playerPointer;
        if (k0 != NULL && k0->control_state == 0x6C) pl = k0;
        else if (k1 != NULL && k1->control_state == 0x6C) { pl = k1; f = 1; }
    }
    sp4C = pl->additional_actor_data;
    levelIndex = getLevelIndex(current_map, 0);
    sp44 = D_global_asm_807FC930[levelIndex] / (f32)D_global_asm_807446C0[levelIndex];
    if (func_global_asm_806BDD90() == 1) {
        D_global_asm_807FDC90->unk2C = func_global_asm_806F8AD4(0, f);
        if (D_global_asm_807FDC90->unk2C != 0) {
            gCurrentActorPointer->control_state = 0x12;
            gCurrentActorPointer->control_state_progress = 0;
            D_global_asm_807FDC90->unk2E = 0x32;
            playActorAnimation(gCurrentActorPointer, 0x28A);
            func_global_asm_8061C518(sp4C->unk104, pl, 4, 0x990, 0x5A, -0x2E, 0x3C, 0xA, 0x20, 0, 0.3000000119f);
            pl->y_rotation = func_global_asm_80665DE0(gCurrentActorPointer->x_position, gCurrentActorPointer->z_position, pl->x_position, pl->z_position);
            pl->unkEE = pl->y_rotation;
        }
    }
    func_global_asm_80724E48(0);
    if (sp44 > 1.0f) {
        sp44 = 1.0f;
    }
    temp_f2 = D_global_asm_807FDC90->unk30 + ((0.44 - D_global_asm_807FDC90->unk30) * sp44);
    if (ACTOR_UNINITIALIZED(gCurrentActorPointer)) {
        func_global_asm_807248B0(gCurrentActorPointer, temp_f2);
    }
    switch (gCurrentActorPointer->control_state) {
        case 0x12:
            func_global_asm_807248B0(gCurrentActorPointer, ((temp_f2 - gCurrentActorPointer->animation_state->scale_x) * 0.1) + gCurrentActorPointer->animation_state->scale_x);
            switch (gCurrentActorPointer->control_state_progress) {
                case 0:
                    if (D_global_asm_807FDC90->unk2E != 0) {
                        D_global_asm_807FDC90->unk2E--;
                        if (D_global_asm_807FDC90->unk2E == 0) {
                            func_global_asm_8061C518(sp4C->unk104, gCurrentActorPointer, 4, 0x305, 0x14A, 3, 0x32, 0x7F, 0x5A, 0, 0.02999999933f);
                        }
                    }
                    if (D_global_asm_807FDC90->unk2C != 0) {
                        if (!(object_timer & 3)) {
                            sp3C = COOP_FED_TNS(current_character_index[f]);
                            D_global_asm_807FDC90->unk2C--;
                            changeCollectableCount(0, f, -1);
                            sp3C[levelIndex]++;
                            for (i = 0; i < 5; i++) { count += COOP_FED_TNS(i)[levelIndex]; }
                            if (count >= D_global_asm_807446C0[levelIndex]) {
                                D_global_asm_807FDC90->unk2C = 0;
                                coop_hud_hide_item(0, 0); // hippo full: nobody has anything left to give, hide both counters
                                coop_hud_hide_item(0, 1);
                            }
                            func_global_asm_8071498C(func_global_asm_8071D0F0);
                            func_global_asm_807149FC(-1);
                            func_global_asm_807149B8(1);
                            func_global_asm_80714950((s32)gCurrentActorPointer);
                            {
                                u8 saved_k_ = current_character_index[0];
                                void *spr_;
                                if (f == 1) current_character_index[0] = current_character_index[1]; // the sprite follows player 0's Kong
                                spr_ = func_global_asm_806FACE8(0);
                                current_character_index[0] = saved_k_;
                                func_global_asm_80714C08(spr_, 0.3f, pl, 4, 0);
                            }
                        }
                    } else if (D_global_asm_807FDC90->unk2E == 0) {
                        D_global_asm_807FDC90->unk2C = 0x1E;
                        gCurrentActorPointer->control_state_progress = 1;
                    }
                    break;
                case 1:
                    if (D_global_asm_807FDC90->unk2C != 0) {
                        D_global_asm_807FDC90->unk2C--;
                    } else {
                        playActorAnimation(gCurrentActorPointer, 0x28B);
                        gCurrentActorPointer->control_state = 0x27;
                        gCurrentActorPointer->control_state_progress = 0;
                    }
                    break;
            }
            break;
        case 0x27:
            if (gCurrentActorPointer->control_state_progress == 1) {
                func_global_asm_80685520(4.0f, 1, gCurrentActorPointer->x_position, gCurrentActorPointer->y_position + 5.0f, gCurrentActorPointer->z_position);
                gCurrentActorPointer->control_state = 0x28;
                gCurrentActorPointer->control_state_progress = 0;
                D_global_asm_807FDC90->unk2C = 0x3C;
            }
            break;
        case 0x28:
            D_global_asm_807FDC90->unk2C -= 1;
            if (D_global_asm_807FDC90->unk2C == 0) {
                gCurrentActorPointer->control_state = 0;
                gCurrentActorPointer->control_state_progress = 0;
                D_global_asm_807FDC90->unk2E = 0;
            }
            break;
    }
    func_global_asm_8072881C(0, (s32 *)&D_global_asm_807FDC90->unk28);
    renderActor(gCurrentActorPointer, 0);
}

// ---- Story co-op: pause menu ----
// Either player can open the pause menu. The menu is driven by the pad of the player who pressed START
// (D_global_asm_807FC8B9) and is shown full screen on that player's view (see coop_fullscreen_now). While the game is
// paused nobody else can act, so the other player cannot pause or move.
extern u8 func_global_asm_8061CB50(void);
extern u8 func_global_asm_8062919C(void);
extern u8 func_global_asm_806291A8(void);
extern u8 D_global_asm_807FC8B9;

RECOMP_PATCH void func_global_asm_806E6234(void) {
    u16 held = *(u16 *)((u8 *)&D_global_asm_807FD610[cc_player_index] + 0x2C);
    if ((global_properties_bitfield & 1) && coop_active_here()) return; // somebody is already pausing: the other player's START does nothing
    if ((held & 0x1000)
        && !func_global_asm_8061CB50()
        && (D_global_asm_807FD888 == 0.0f)
        && (gameIsInAdventureMode() || gameIsInMysteryMenuMinigameMode() || gameIsInDKTVMode())
        && !func_global_asm_8062919C()
        && !func_global_asm_806291A8()) {
        { extern void coop_hud_peek_cancel(void); coop_hud_peek_cancel(); }
        D_global_asm_807FC8B9 = cc_player_index;
        global_properties_bitfield |= 1;
    }
}

// ---- Story co-op: update of the actors while the game is paused (func_global_asm_806789E4) ----
// Only the pause menu actors run here (unk64 & 2). The main pause menu reads the inputs of player 0
// (character_change_array[0].new_controller_inputs): when P2 pressed START it is given P2's inputs for its update.
extern void *D_global_asm_807FBFE0[];
extern void func_global_asm_80614A64(Actor *);
extern void func_global_asm_806790F4(u8);
extern void func_global_asm_80678B04(void);

// While the pause menu of P2 is updated / drawn, the menu (which reads player 0's data) is shown P2's Kong and
// consumables: current_character_index[0] and the per-player fields of PlayerProgress (ammo / oranges / crystals / film at
// +0x2F0, health at +0x2FB, melons at +0x2FC) are exchanged with P1's, and put back right after.
extern u8 D_global_asm_807FC950[];
void coop_pause_persona(int on) {
    static int active = 0;
    u8 tmp;
    int i;
    if (on == active) return;
    if (on && !(coop_active_here() && (D_global_asm_807FC8B9 & 1) && character_change_array[1].does_player_exist)) return;
    tmp = current_character_index[0];
    current_character_index[0] = current_character_index[1];
    current_character_index[1] = tmp;
    for (i = 0x2F0; i < 0x2FE; i++) {
        tmp = D_global_asm_807FC950[i];
        D_global_asm_807FC950[i] = D_global_asm_807FC950[0x306 + i];
        D_global_asm_807FC950[0x306 + i] = tmp;
    }
    active = on;
}

RECOMP_PATCH void func_global_asm_806789E4(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_global_asm_807FBFE0[i] = NULL;
    }
    for (D_global_asm_807FBB3C = 0; D_global_asm_807FBB3C < D_global_asm_807FBB34; D_global_asm_807FBB3C++) {
        gCurrentActorPointer = D_global_asm_807FB930[D_global_asm_807FBB3C].unk0;
        if (gCurrentActorPointer->unk64 & 2) {
            int pm_swap_ = 0;
            func_global_asm_80678BBC(gCurrentActorPointer);
            if (coop_active_here() && (D_global_asm_807FC8B9 & 1) && character_change_array[1].does_player_exist
                && (void *)D_global_asm_8074C0A0[gCurrentActorPointer->unk58] == (void *)func_global_asm_806A8158) {
                __typeof__(character_change_array[0].new_controller_inputs) t_new_ = character_change_array[0].new_controller_inputs;
                character_change_array[0].new_controller_inputs = character_change_array[1].new_controller_inputs;
                character_change_array[1].new_controller_inputs = t_new_;
                {   // the stick is read straight from the pad array (port 0)
                    __typeof__(D_global_asm_807ECD10[0]) t_pad_ = D_global_asm_807ECD10[0];
                    D_global_asm_807ECD10[0] = D_global_asm_807ECD10[1];
                    D_global_asm_807ECD10[1] = t_pad_;
                }
                pm_swap_ = 1;
            }
            // pause menu of a bonus barrel played by P2 (single-player map): the menu reads controller 1 -> swap the two pads
            int mg_pause_ = 0;
            if (coop_mg_swapped && !coop_map_allowed() && (void *)D_global_asm_8074C0A0[gCurrentActorPointer->unk58] == (void *)func_global_asm_806A8158) {
                OSContPad tp_ = D_global_asm_807ECD10[0];
                u16 tn_ = newly_pressed_input[0];
                D_global_asm_807ECD10[0] = D_global_asm_807ECD10[1];
                D_global_asm_807ECD10[1] = tp_;
                newly_pressed_input[0] = newly_pressed_input[1];
                newly_pressed_input[1] = tn_;
                mg_pause_ = 1;
            }
            if (pm_swap_) coop_pause_persona(1);
            D_global_asm_8074C0A0[gCurrentActorPointer->unk58]();
            if (pm_swap_) coop_pause_persona(0);
            if (mg_pause_) {
                OSContPad tp_ = D_global_asm_807ECD10[0];
                u16 tn_ = newly_pressed_input[0];
                D_global_asm_807ECD10[0] = D_global_asm_807ECD10[1];
                D_global_asm_807ECD10[1] = tp_;
                newly_pressed_input[0] = newly_pressed_input[1];
                newly_pressed_input[1] = tn_;
            }
            if (pm_swap_) {
                __typeof__(character_change_array[0].new_controller_inputs) t_new_ = character_change_array[0].new_controller_inputs;
                character_change_array[0].new_controller_inputs = character_change_array[1].new_controller_inputs;
                character_change_array[1].new_controller_inputs = t_new_;
                {
                    __typeof__(D_global_asm_807ECD10[0]) t_pad_ = D_global_asm_807ECD10[0];
                    D_global_asm_807ECD10[0] = D_global_asm_807ECD10[1];
                    D_global_asm_807ECD10[1] = t_pad_;
                }
            }
            func_global_asm_80678CC8(gCurrentActorPointer);
        }
        if (gCurrentActorPointer->animation_state != NULL) {
            func_global_asm_80614A64(gCurrentActorPointer);
        }
    }
    func_global_asm_806790F4(1);
    func_global_asm_80678B04();
}