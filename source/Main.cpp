#include "plugin.h"
#include "common.h"
#include "Rage.h"
#include "CTimer.h"
#include "CText.h"
#include "CRadar.h"
#include "CDrawSpriteDC.h"
#include "CDrawRectDC.h"
#include "T_CB_Generic.h"
#include "CTxdStore.h"
#include "CFont.h"
#include "CWeaponInfo.h"
#include "CWorld.h"
#include "CPad.h"
#include "CHud.h"
#include "CHudColours.h"
#include "CMenuManager.h"
#include "CClock.h"
#include "CSetCurrentViewportToNULL.h"
#include "CModelInfo.h"
#include "CPools.h"
#include "CScriptCommands.h"
#include "CTaskSimpleAimGun.h"
#include "CTaskComplexAimAndThrowProjectile.h"
#include "CCutsceneMgr.h"
#include "Utility.h"
#include "Audio.h"
#include "CCamera.h"
#include "CUserDisplay.h"
#include "CControlMgr.h"

#include "SpriteLoader.h"

#include "extensions/ScriptCommands.h"

#include "dxsdk/d3d9.h"

#include "debugmenu_public.h"

DebugMenuAPI gDebugMenuAPI;

bool (*DebugMenuShowing)();
void (*DebugMenuPrintString)(const char* str, float x, float y, int style);
int (*DebugMenuGetStringSize)(const char* str);

class ClassicHudIV {
public:
    static inline plugin::SpriteLoader spriteLoader = {};
    static inline int16_t fontSize[4][210] = { 0 };

    // Settings
    static inline bool NO_MONEY_COUNTER_ZEROES = false;
    static inline float HUD_SCALE = 0.8f;
	static inline bool RENDER_PROGRESS_BARS = true;

    enum {
        HUD_III,
        HUD_VC,
        HUD_SA,
    };

    enum {
        HUDELEMENT_CLOCK,
        HUDELEMENT_MONEY,
        HUDELEMENT_WEAPON,
        HUDELEMENT_AMMO,
        HUDELEMENT_HEALTH,
        HUDELEMENT_HEALTH_ICON,
        HUDELEMENT_ARMOUR,
        HUDELEMENT_ARMOUR_ICON,
        HUDELEMENT_WANTED,
        HUDELEMENT_NOT_WANTED,
        HUDELEMENT_ZONE,
        HUDELEMENT_STREET,
        HUDELEMENT_VEHICLE,
        HUDELEMENT_RADAR_DISC,
        NUM_HUDELEMENTS
    };

    struct HudElement {
        float x;
        float y;
        float w;
        float h;
        float extraX;
        float extraY;
        uint8_t align;
        uint8_t font;
        bool shadow;
        bool outline;
        CRGBA color;
        CRGBA extracolor;
        std::string str;
    };

    static inline std::array<HudElement, NUM_HUDELEMENTS> elements = {};

    static void LoadSettings() {
		plugin::config_file config(true, false);

		NO_MONEY_COUNTER_ZEROES = config["NoMoneyCounterZeroes"].asBool(NO_MONEY_COUNTER_ZEROES);
        HUD_SCALE = config["HudScale"].asFloat(HUD_SCALE);
        RENDER_PROGRESS_BARS = config["RenderProgressBars"].asBool(RENDER_PROGRESS_BARS);
    }

    static void LoadFontData(std::string const& filename) {
        std::ifstream file(filename);
        if (!file.is_open())
            return;

        std::string line;
        int32_t fontIdx = -1;
        int32_t valueIdx = 0;

        while (std::getline(file, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#')
                continue;

            if (line.substr(0, 4) == "font") {
                if (fontIdx >= 1)
                    break;
                fontIdx++;
                valueIdx = 0;
                continue;
            }
            if (line == "end") {
                continue;
            }
            if (fontIdx < 0 || fontIdx > 1)
                continue;

            std::istringstream iss(line);
            int16_t val;
            while (iss >> val) {
                if (valueIdx < 210)
                    fontSize[fontIdx][valueIdx++] = val;
            }
        }
    }

    static void LoadHudDetails(std::string const& filename) {
        std::ifstream file(filename);
        if (!file.is_open())
            return;

        std::string line;
        size_t idx = 0;
        while (std::getline(file, line) && idx < elements.size()) {
            if (line.empty() || line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string name;
            float x, y, w, h, extraX, extraY;
            int32_t font, align, shadow, outline;
            int32_t cr, cg, cb, ca, exr, exg, exb, exa;

            if (!(iss >> name
                  >> x >> y >> w >> h
                  >> extraX >> extraY
                  >> font >> align >> shadow >> outline
                  >> cr >> cg >> cb >> ca
                  >> exr >> exg >> exb >> exa)) {
                continue;
            }

            std::string charField = {};
            iss >> charField;

            HudElement& elem = elements[idx++];
            elem.x = x;
            elem.y = y;
            elem.w = w;
            elem.h = h;
            elem.extraX = extraX;
            elem.extraY = extraY;
            elem.align = static_cast<uint8_t>(align);
            elem.font = static_cast<uint8_t>(font);
            elem.shadow = static_cast<uint8_t>(shadow);
            elem.outline = static_cast<uint8_t>(outline);

            elem.color = CRGBA(static_cast<unsigned char>(cr), static_cast<unsigned char>(cg), static_cast<unsigned char>(cb), static_cast<unsigned char>(ca));
            elem.extracolor = CRGBA(static_cast<unsigned char>(exr), static_cast<unsigned char>(exg), static_cast<unsigned char>(exb), static_cast<unsigned char>(exa));
            elem.str = charField;
        }
    }

    static wchar_t FindNewCharacter(wchar_t c) {
        if (c >= 16 && c <= 26) return c + 128;
        if (c >= 8 && c <= 9) return c + 86;
        if (c == 4) return c + 89;
        if (c == 7) return 206;
        if (c == 14) return 207;
        if (c >= 33 && c <= 58) return c + 122;
        if (c >= 65 && c <= 90) return c + 90;
        if (c >= 96 && c <= 118) return c + 85;
        if (c >= 119 && c <= 140) return c + 62;
        if (c >= 141 && c <= 142) return 204;
        if (c == 143) return 205;
        if (c == 1) return 208;
        return c;
    }

    struct StringParams {
        float w;
        float h;
        int32_t font;
        int32_t align;
        bool prop;
        bool shadow;
        bool outline;
        rage::Color32 color;
        rage::Color32 dropColor;
        bool slant;
        StringParams(float w, float h, int32_t font, int32_t align, bool prop, bool shadow, bool outline, rage::Color32 const& color, rage::Color32 const& dropColor, bool slant = false)
            : w(w), h(h), font(font), align(align), prop(prop), shadow(shadow), outline(outline), color(color), dropColor(dropColor), slant(slant) {
        }
    };

    static void PrintString(std::wstring str, float x, float y, StringParams const& params) {
        auto getCharWidth = [&](char c) -> float {
            int32_t font = params.font;
            if (font > 1)
                font = 1;

            if (params.prop) {
                return fontSize[font][c] * params.w;
            }
            else {
                return fontSize[font][209] * params.w;
            }
        };

        static CSprite2d sprite;
        switch (params.font) {
            case 0:
                sprite = spriteLoader.GetSprite("font2");
                break;
            default:
                sprite = spriteLoader.GetSprite("font1");
                break;
        }

        bool fontHalfTex = params.font == 2;
        rage::fwRect rect = { x, y, x + 32.0f * params.w * 1.0f, y + 40.0f * params.h * 0.5f };

        if (params.align == 2) {
            for (auto& it : str) {
                char c = it - L' ';
                rect.left -= getCharWidth(c);
                rect.right -= getCharWidth(c);
            }
        }
        else if (params.align == 1) {
            for (auto& it : str) {
                char c = it - L' ';
                rect.left -= getCharWidth(c) / 2;
                rect.right -= getCharWidth(c) / 2;
            }
        }

        if (!sprite.m_pTexture)
            return;

        sprite.SetRenderState();
        for (auto& it : str) {
            wchar_t c = it - L' ';

            if (fontHalfTex) {
                c = FindNewCharacter(c);
            }

            float xoff = c % 16;
            float yoff = c / 16;
            rage::fwRect uv = {
                xoff / 16.0f + 0.00125f, yoff / 12.8f + 0.0015f, (xoff + 1.0f) / 16.0f - 0.00125f, (yoff + 1.0f) / 12.8f - 0.0015f
            };

            if (params.shadow) {
                rage::fwRect shad = rect;
                shad.Translate(ScaleX(2.0f), ScaleY(2.0f));
                CSprite2d::Draw(shad, uv, params.dropColor);
            }

            if (params.outline) {
                float outlineSize = 1.0f;
                constexpr int numOffsets = 16;
                float angleStep = outlineSize * 3.14159265359f / numOffsets;

                for (int32_t i = 0; i < numOffsets; i++) {
                    float angle = i * angleStep;
                    rage::fwRect edge = rect;
                    edge.Translate(ScaleX(outlineSize * std::cos(angle)), ScaleY(outlineSize * std::sin(angle)));
                    CSprite2d::Draw(edge, uv, params.dropColor);
                }
            }

            CSprite2d::Draw(rect, uv, params.color);

            float charWidth = getCharWidth(c);
            rect.left += charWidth;
            rect.right += charWidth;

            if (params.slant) {
                rect.top -= params.h * 2.0f;
                rect.bottom -= params.h * 2.0f;
            }
        }
        CSprite2d::ClearRenderState();
    }

    static void PrintString(std::string str, float x, float y, StringParams const& params) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide = converter.from_bytes(str);
        PrintString(wide, x, y, params);
    }

    static void DrawWanted() {
        auto player = FindPlayerPed(0);

        for (int32_t i = 0; i < 6; i++) {
            if (player->m_pPlayerInfo->m_PlayerData.GetWantedLevel() > 5 - i
                && (CTimer::GetTimeInMilliseconds() > player->m_pPlayerInfo->m_PlayerData.m_Wanted.m_nLastWantedLevelChange + 2000 || FLASH_ITEM(500, 250)))
                PrintString(elements[HUDELEMENT_WANTED].str, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_WANTED].x + ((5 - i) * elements[HUDELEMENT_WANTED].extraX)), ScaleY(elements[HUDELEMENT_WANTED].y), StringParams(ScaleX(elements[HUDELEMENT_WANTED].w), ScaleY(elements[HUDELEMENT_WANTED].h), elements[HUDELEMENT_WANTED].font, elements[HUDELEMENT_WANTED].align, true, elements[HUDELEMENT_WANTED].shadow, elements[HUDELEMENT_WANTED].outline, elements[HUDELEMENT_WANTED].color, elements[HUDELEMENT_WANTED].extracolor));
            else
                PrintString(elements[HUDELEMENT_NOT_WANTED].str, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_NOT_WANTED].x + ((5 - i) * elements[HUDELEMENT_NOT_WANTED].extraX)), ScaleY(elements[HUDELEMENT_NOT_WANTED].y), StringParams(ScaleX(elements[HUDELEMENT_NOT_WANTED].w), ScaleY(elements[HUDELEMENT_NOT_WANTED].h), elements[HUDELEMENT_NOT_WANTED].font, elements[HUDELEMENT_NOT_WANTED].align, true, elements[HUDELEMENT_NOT_WANTED].shadow, elements[HUDELEMENT_NOT_WANTED].outline, elements[HUDELEMENT_NOT_WANTED].color, elements[HUDELEMENT_NOT_WANTED].extracolor));
        }
    }

    static inline std::wstring currStreetName = {};
    static inline std::wstring currAreaName = {};
    static inline std::wstring currVehicleName = {};

    static inline int32_t areaNameTimer = 0;
    static inline int32_t streetNameTimer = 0;
    static inline int32_t vehicleNameTimer = 0;

    static inline const uint32_t FADING_TEXT_TIME = 3500;

    static uint8_t CalculateFadeTextAlpha(int32_t& timer) {
        int32_t timeLeft = timer - CTimer::GetTimeInMilliseconds();

        if (timeLeft <= 0) {
            return 0;
        }

        constexpr int32_t fadeTime = 500;
        constexpr int32_t fullTime = FADING_TEXT_TIME - 2 * fadeTime;

        int32_t timeSinceAppear = FADING_TEXT_TIME - timeLeft;
        uint8_t alpha = 255;

        if (timeSinceAppear < fadeTime) {
            // Fade in
            alpha = static_cast<uint8_t>((timeSinceAppear / (float)fadeTime) * 255.0f);
        }
        else if (timeSinceAppear > (fadeTime + fullTime)) {
            // Fade out
            int32_t fadeOutElapsed = timeSinceAppear - (fadeTime + fullTime);
            alpha = static_cast<uint8_t>(255.0f * (1.0f - (fadeOutElapsed / (float)fadeTime)));
        }
        else {
            // Fully visible
            alpha = 255;
        }

        return alpha;
    }

    static bool RadarZoomedOut() {
        return CRadar::m_radarRange == 960.0f;
    }

    static void DrawAreaName() {
        wchar_t* areaName = CUserDisplay::DisplayAreaName.m_CurrName;
        bool force = RadarZoomedOut();

        if (currAreaName.compare(areaName) != 0) {
            currAreaName.assign(areaName);
            areaNameTimer = CTimer::GetTimeInMilliseconds() + FADING_TEXT_TIME;
        }

        if (force)
            areaNameTimer = CTimer::GetTimeInMilliseconds() + (FADING_TEXT_TIME / 2);

        uint8_t alpha = CalculateFadeTextAlpha(areaNameTimer);
        if (force)
            alpha = 255;

        CRGBA areaColor = elements[HUDELEMENT_ZONE].color;
        areaColor.a = alpha;

        PrintString(currAreaName,
                    SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_ZONE].x),
                    SCREEN_HEIGHT - ScaleY(elements[HUDELEMENT_ZONE].y),
                    StringParams(ScaleX(elements[HUDELEMENT_ZONE].w),
                                 ScaleY(elements[HUDELEMENT_ZONE].h),
                                 elements[HUDELEMENT_ZONE].font,
                                 elements[HUDELEMENT_ZONE].align,
                                 true,
                                 elements[HUDELEMENT_ZONE].shadow,
                                 elements[HUDELEMENT_ZONE].outline,
                                 areaColor, CRGBA(0, 0, 0, alpha), true));
    }

    static void DrawStreetName() {
        wchar_t* streetName = CUserDisplay::DisplayStreetName.m_CurrName;
        bool force = RadarZoomedOut();

        if (currStreetName.compare(streetName) != 0) {
            currStreetName.assign(streetName);
            streetNameTimer = CTimer::GetTimeInMilliseconds() + FADING_TEXT_TIME;
        }

        if (force)
            streetNameTimer = CTimer::GetTimeInMilliseconds() + (FADING_TEXT_TIME / 2);

        uint8_t alpha = CalculateFadeTextAlpha(streetNameTimer);
        if (force)
            alpha = 255;

        CRGBA color = elements[HUDELEMENT_STREET].color;
        color.a = alpha;

        PrintString(currStreetName,
                    SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_STREET].x),
                    SCREEN_HEIGHT - ScaleY(elements[HUDELEMENT_STREET].y),
                    StringParams(ScaleX(elements[HUDELEMENT_STREET].w),
                                 ScaleY(elements[HUDELEMENT_STREET].h),
                                 elements[HUDELEMENT_STREET].font,
                                 elements[HUDELEMENT_STREET].align,
                                 true,
                                 elements[HUDELEMENT_STREET].shadow,
                                 elements[HUDELEMENT_STREET].outline,
                                 color,
                                 CRGBA(0, 0, 0, alpha),
                                 true));
    }

    static void DrawVehicleName() {
        wchar_t* vehicleName = CUserDisplay::DisplayVehicleName.m_CurrName;
        bool force = RadarZoomedOut();

        if (currVehicleName.compare(vehicleName) != 0) {
            currVehicleName.assign(vehicleName);
            vehicleNameTimer = CTimer::GetTimeInMilliseconds() + FADING_TEXT_TIME;
        }

        if (force)
            vehicleNameTimer = CTimer::GetTimeInMilliseconds() + (FADING_TEXT_TIME / 2);

        uint8_t alpha = CalculateFadeTextAlpha(vehicleNameTimer);
        if (force)
            alpha = 255;

        CRGBA color = elements[HUDELEMENT_VEHICLE].color;
        color.a = alpha;

        PrintString(currVehicleName,
                    SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_VEHICLE].x),
                    SCREEN_HEIGHT - ScaleY(elements[HUDELEMENT_VEHICLE].y),
                    StringParams(ScaleX(elements[HUDELEMENT_VEHICLE].w),
                                 ScaleY(elements[HUDELEMENT_VEHICLE].h),
                                 elements[HUDELEMENT_VEHICLE].font,
                                 elements[HUDELEMENT_VEHICLE].align,
                                 true,
                                 elements[HUDELEMENT_VEHICLE].shadow,
                                 elements[HUDELEMENT_VEHICLE].outline,
                                 color,
                                 CRGBA(0, 0, 0, alpha),
                                 false));
    }

    static void DrawWeaponIconAndAmmo() {
        auto playa = FindPlayerPed(0);
        auto& weaponData = playa->m_WeaponData;
        auto& currentWeapon = weaponData.m_aWeapons[weaponData.m_nActiveWeaponSlot];
        auto& weaponType = currentWeapon.m_nType;
        auto ammoData = weaponData.GetAmmoDataExtraCheck();
        auto weaponInfo = CWeaponInfo::GetWeaponInfo(weaponType);
        auto object = weaponData.m_pHoldingObject;
        auto control = playa->GetControlFromPlayer();

        // Weapon Icon
        CSprite2d sprite;
        switch (weaponType) {
            default:
                sprite = spriteLoader.GetSprite("frame");
                break;
            case WEAPONTYPE_UNARMED:
                sprite = spriteLoader.GetSprite("unarmed");
                break;
            case WEAPONTYPE_BASEBALLBAT:
                sprite = spriteLoader.GetSprite("bat");
                break;
            case WEAPONTYPE_POOLCUE:
                sprite = spriteLoader.GetSprite("poolcue");
                break;
            case WEAPONTYPE_KNIFE:
                sprite = spriteLoader.GetSprite("knifecur");
                break;
            case WEAPONTYPE_GRENADE:
                sprite = control->m_inputs[rage::INPUT_ATTACK].m_nNewState && !control->m_inputs[rage::INPUT_AIM].m_nNewState ? spriteLoader.GetSprite("unarmedg") : spriteLoader.GetSprite("grenade");
                break;
            case WEAPONTYPE_MOLOTOV:
                sprite = spriteLoader.GetSprite("molotov");
                break;
                //case WEAPONTYPE_ROCKET:
                //   break;
            case WEAPONTYPE_PISTOL:
                sprite = spriteLoader.GetSprite("colt45");
                break;
                //case WEAPONTYPE_UNUSED0:
                //    break;
            case WEAPONTYPE_DEAGLE:
                sprite = spriteLoader.GetSprite("deagle");
                break;
            case WEAPONTYPE_SHOTGUN:
                sprite = spriteLoader.GetSprite("shotgun");
                break;
            case WEAPONTYPE_BERETTA:
                sprite = spriteLoader.GetSprite("beretta");
                break;
            case WEAPONTYPE_MICRO_UZI:
                sprite = spriteLoader.GetSprite("uzi");
                break;
            case WEAPONTYPE_MP5:
                sprite = spriteLoader.GetSprite("mp5lng");
                break;
            case WEAPONTYPE_AK47:
                sprite = spriteLoader.GetSprite("ak47");
                break;
            case WEAPONTYPE_M4:
                sprite = spriteLoader.GetSprite("m4");
                break;
            case WEAPONTYPE_SNIPERRIFLE:
                sprite = spriteLoader.GetSprite("sniper");
                break;
            case WEAPONTYPE_M40A1:
                sprite = spriteLoader.GetSprite("m40a1");
                break;
            case WEAPONTYPE_RLAUNCHER:
                sprite = spriteLoader.GetSprite("rocketla");
                break;
            case WEAPONTYPE_FTHROWER:
                sprite = spriteLoader.GetSprite("flame");
                break;
            case WEAPONTYPE_MINIGUN:
                sprite = spriteLoader.GetSprite("minigun");
                break;
            case WEAPONTYPE_GRENADE_LAUNCHER:
                sprite = spriteLoader.GetSprite("grenadela");
                break;
            case WEAPONTYPE_ASSAULT_SHOTGUN:
                sprite = spriteLoader.GetSprite("assaultshotgun");
                break;
            case WEAPONTYPE_SAWN_OFF_SHOTGUN:
                sprite = spriteLoader.GetSprite("sawnoff");
                break;
            case WEAPONTYPE_AUTOMATIC_PISTOL:
                sprite = spriteLoader.GetSprite("a9mm");
                break;
            case WEAPONTYPE_PIPE_BOMB:
                sprite = spriteLoader.GetSprite("pipebomb");
                break;
            case WEAPONTYPE_PISTOL_44:
                sprite = spriteLoader.GetSprite("pistol44");
                break;
            case WEAPONTYPE_AA12:
                sprite = spriteLoader.GetSprite("aa12");
                break;
            case WEAPONTYPE_AA12_EXPLOSIVE_SHELLS:
                sprite = spriteLoader.GetSprite("aa12");
                break;
            case WEAPONTYPE_P90:
                sprite = spriteLoader.GetSprite("p90");
                break;
            case WEAPONTYPE_GOLDEN_UZI:
                sprite = spriteLoader.GetSprite("golduzi");
                break;
            case WEAPONTYPE_M249:
                sprite = spriteLoader.GetSprite("m249");
                break;
            case WEAPONTYPE_ADVANCED_SNIPER:
                sprite = spriteLoader.GetSprite("advancedsniper");
                break;
            case WEAPONTYPE_STICKY_BOMB:
                sprite = spriteLoader.GetSprite("stickybomb");
                break;
            case WEAPONTYPE_PARACHUTE:
                sprite = spriteLoader.GetSprite("parachute");
                break;
            case WEAPONTYPE_CAMERA:
                sprite = spriteLoader.GetSprite("camera");
                break;
            case WEAPONTYPE_OBJECT: {
                sprite = spriteLoader.GetSprite("unarmedt");

                if (object) {
                    int16_t index = object->m_nModelIndex;
                    auto hash = CModelInfo::ms_modelInfoPtrs[index]->m_nHash;
                    switch (hash) {
                    case eModelHashes::MODEL_HASH_CJ_MOBILE_HAND_1:
                        sprite = spriteLoader.GetSprite("unarmed");
                        break;
                    default:
                        break;
                    }
                }
            } break;
        }

        if (sprite.m_pTexture) {
            sprite.SetRenderState();
            CSprite2d::Draw(rage::fwRect(SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_WEAPON].x), ScaleY(elements[HUDELEMENT_WEAPON].y), SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_WEAPON].x - elements[HUDELEMENT_WEAPON].w), ScaleY(elements[HUDELEMENT_WEAPON].y + elements[HUDELEMENT_WEAPON].h)), rage::Color32(255, 255, 255, 255));
            CSprite2d::ClearRenderState();
        }

        // Ammo
        if (weaponInfo->m_nDamageType != DAMAGETYPE_MELEE && weaponType != WEAPONTYPE_OBJECT) {
            char buf[32];
            auto clipSize = CWeaponInfo::GetWeaponInfo(weaponType)->m_nClipSize;

            if (ammoData) {
                uint32_t ammoInClip = ammoData->m_nAmmoInClip;
                uint32_t ammoTotal = ammoData->m_nAmmoTotal;
                uint32_t ammo, clip;

                if (clipSize <= 1 || clipSize >= 1000)
                    sprintf(buf, "%d", ammoTotal);
                else {
                    clip = ammoInClip;
                    ammo = std::min(ammoTotal - ammoInClip, (uint32_t)9999);
                    sprintf(buf, "%d-%d", ammo, clip);
                }
            }
            else {
                int32_t ammoTotal = weaponData.GetAmountOfAmmunition(weaponInfo->m_nSlot);
                int32_t prevAmmoTotal = ammoTotal;
                if (clipSize < ammoTotal)
                    ammoTotal = clipSize;

                int32_t ammo = prevAmmoTotal - ammoTotal;
                int32_t clip = ammoTotal;
                if (clipSize <= 1 || clipSize >= 1000) {
                    sprintf(buf, "%d", ammo + 1);
                }
                else {
                    ammo = std::min(ammo, 9999);
                    sprintf(buf, "%d-%d", ammo, clip);
                }
            }
            PrintString(buf, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_AMMO].x), ScaleY(elements[HUDELEMENT_AMMO].y), StringParams(ScaleX(elements[HUDELEMENT_AMMO].w), ScaleY(elements[HUDELEMENT_AMMO].h), elements[HUDELEMENT_AMMO].font, elements[HUDELEMENT_AMMO].align, true, elements[HUDELEMENT_AMMO].shadow, elements[HUDELEMENT_AMMO].outline, elements[HUDELEMENT_AMMO].color, elements[HUDELEMENT_AMMO].extracolor));
        }
    }

    static void DrawMoneyAndClock() {
        auto playa = FindPlayerPed(0);
        char buf[32] = { 0 };

        sprintf(buf, NO_MONEY_COUNTER_ZEROES ? "$%d" : "$%08d", playa->m_pPlayerInfo->m_nDisplayMoney);
        PrintString(buf, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_MONEY].x), ScaleY(elements[HUDELEMENT_MONEY].y), StringParams(ScaleX(elements[HUDELEMENT_MONEY].w), ScaleY(elements[HUDELEMENT_MONEY].h), elements[HUDELEMENT_MONEY].font, elements[HUDELEMENT_MONEY].align, false, elements[HUDELEMENT_MONEY].shadow, elements[HUDELEMENT_MONEY].outline, elements[HUDELEMENT_MONEY].color, elements[HUDELEMENT_MONEY].extracolor));

        sprintf(buf, "%02d:%02d", CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
        PrintString(buf, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_CLOCK].x), ScaleY(elements[HUDELEMENT_CLOCK].y), StringParams(ScaleX(elements[HUDELEMENT_CLOCK].w), ScaleY(elements[HUDELEMENT_CLOCK].h), elements[HUDELEMENT_CLOCK].font, elements[HUDELEMENT_CLOCK].align, false, elements[HUDELEMENT_CLOCK].shadow, elements[HUDELEMENT_CLOCK].outline, elements[HUDELEMENT_CLOCK].color, elements[HUDELEMENT_CLOCK].extracolor));
    }

    static inline float lastPlayerHealth = 200.0f;
    static inline float lastPlayerArmour = 200.0f;

    static inline int32_t timeToFlashHealth = 0;
    static inline int32_t timeToFlashArmour = 0;

    static void DrawHealthAndArmour() {
        auto playa = FindPlayerPed(0);

        // Code to make health and armour flash when value changes
        if (playa->m_fHealth != lastPlayerHealth) {
            lastPlayerHealth = playa->m_fHealth;
            timeToFlashHealth = 1000 + CTimer::GetTimeInMilliseconds();
        }

        if (playa->m_fArmour != lastPlayerArmour) {
            lastPlayerArmour = playa->m_fArmour;
            timeToFlashArmour = 1000 + CTimer::GetTimeInMilliseconds();
        }
        //

        {
            bool renderHealth = true;
            if (timeToFlashHealth > CTimer::GetTimeInMilliseconds())
                renderHealth = FLASH_ITEM(200, 100);

            if (playa->m_fHealth <= 10.0f)
                renderHealth = FLASH_ITEM(500, 250);

            char buf[32] = { 0 };
            float progress = ((playa->m_fHealth + 0.5f) / playa->m_pPlayerInfo->MaxHealth) * 100.0f;
            if (renderHealth) {
                if (RENDER_PROGRESS_BARS) {
                    DrawProgressBar(
                        SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_HEALTH].x) - ScaleX(elements[HUDELEMENT_HEALTH].w),
                        ScaleY(elements[HUDELEMENT_HEALTH].y),
                        ScaleX(elements[HUDELEMENT_HEALTH].w),
                        ScaleY(elements[HUDELEMENT_HEALTH].h),
                        progress / 100.0f,
                        elements[HUDELEMENT_HEALTH].shadow,
                        elements[HUDELEMENT_HEALTH].outline,
                        elements[HUDELEMENT_HEALTH].color,
                        elements[HUDELEMENT_HEALTH].extracolor);
                }
                else {
                    sprintf(buf, "%03d", (int32_t)progress);
                    PrintString(buf, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_HEALTH].x), ScaleY(elements[HUDELEMENT_HEALTH].y), StringParams(ScaleX(elements[HUDELEMENT_HEALTH].w), ScaleY(elements[HUDELEMENT_HEALTH].h), elements[HUDELEMENT_HEALTH].font, elements[HUDELEMENT_HEALTH].align, false, elements[HUDELEMENT_HEALTH].shadow, elements[HUDELEMENT_HEALTH].outline, elements[HUDELEMENT_HEALTH].color, elements[HUDELEMENT_HEALTH].extracolor));
                }

                PrintString(elements[HUDELEMENT_HEALTH_ICON].str, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_HEALTH_ICON].x), ScaleY(elements[HUDELEMENT_HEALTH_ICON].y), StringParams(ScaleX(elements[HUDELEMENT_HEALTH_ICON].w), ScaleY(elements[HUDELEMENT_HEALTH_ICON].h), elements[HUDELEMENT_HEALTH_ICON].font, elements[HUDELEMENT_HEALTH_ICON].align, false, elements[HUDELEMENT_HEALTH_ICON].shadow, elements[HUDELEMENT_HEALTH_ICON].outline, elements[HUDELEMENT_HEALTH_ICON].color, elements[HUDELEMENT_HEALTH_ICON].extracolor));
            }
        }

        {
            float progress = ((playa->m_fArmour + 0.5f) / playa->m_pPlayerInfo->MaxArmour) * 100.0f;
            if (playa->m_fArmour > 1.0f) {
                bool renderArmour = true;
                if (timeToFlashArmour > CTimer::GetTimeInMilliseconds())
                    renderArmour = FLASH_ITEM(200, 100);

                if (renderArmour) {
                    if (RENDER_PROGRESS_BARS) {
                        DrawProgressBar(
                            SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_ARMOUR].x) - ScaleX(elements[HUDELEMENT_ARMOUR].w),
                            ScaleY(elements[HUDELEMENT_ARMOUR].y),
                            ScaleX(elements[HUDELEMENT_ARMOUR].w),
                            ScaleY(elements[HUDELEMENT_ARMOUR].h),
                            progress / 100.0f,
                            elements[HUDELEMENT_ARMOUR].shadow,
                            elements[HUDELEMENT_ARMOUR].outline,
                            elements[HUDELEMENT_ARMOUR].color,
                            elements[HUDELEMENT_ARMOUR].extracolor);
                    }
                    else {
                        char buf[32] = { 0 };

                        sprintf(buf, "%03d", (int32_t)progress);
                        PrintString(buf, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_ARMOUR].x), ScaleY(elements[HUDELEMENT_ARMOUR].y), StringParams(ScaleX(elements[HUDELEMENT_ARMOUR].w), ScaleY(elements[HUDELEMENT_ARMOUR].h), elements[HUDELEMENT_ARMOUR].font, elements[HUDELEMENT_ARMOUR].align, false, elements[HUDELEMENT_ARMOUR].shadow, elements[HUDELEMENT_ARMOUR].outline, elements[HUDELEMENT_ARMOUR].color, elements[HUDELEMENT_ARMOUR].extracolor));
                    }

                    PrintString(elements[HUDELEMENT_ARMOUR_ICON].str, SCREEN_WIDTH - ScaleX(elements[HUDELEMENT_ARMOUR_ICON].x), ScaleY(elements[HUDELEMENT_ARMOUR_ICON].y), StringParams(ScaleX(elements[HUDELEMENT_ARMOUR_ICON].w), ScaleY(elements[HUDELEMENT_ARMOUR_ICON].h), elements[HUDELEMENT_ARMOUR_ICON].font, elements[HUDELEMENT_ARMOUR_ICON].align, false, elements[HUDELEMENT_ARMOUR_ICON].shadow, elements[HUDELEMENT_ARMOUR_ICON].outline, elements[HUDELEMENT_ARMOUR_ICON].color, elements[HUDELEMENT_ARMOUR_ICON].extracolor));
                }
            }
        }
    }

    static void DrawHudCB() {
        auto dev = GetD3DDevice<IDirect3DDevice9>();
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

        SetScaleMult(HUD_SCALE);
        auto playa = FindPlayerPed(0);
        if (playa) {
            DrawWeaponIconAndAmmo();
            DrawMoneyAndClock();
            DrawHealthAndArmour();
            DrawWanted();
        }

        if (!CPlayerInfo::ms_bDisplayingPhone) {
            DrawAreaName();
            //DrawStreetName();
            DrawVehicleName();
        }

        SetScaleMult();
        //DrawDebugStuff();
    }

    static void DrawHud() {
        if (CCutsceneMgr::IsRunning() ||
            CHud::HideAllComponents ||
            CHud::HideAllComponentsThisFrame ||
            CMenuManager::m_MenuActive ||
            TheViewport.m_bWidescreen)
            return;

        if (plugin::scripting::CallCommandById<bool>(plugin::Commands::ARE_WIDESCREEN_BORDERS_ACTIVE) ||
            plugin::scripting::CallCommandById<bool>(plugin::Commands::IS_FRONTEND_FADING))
            return;

        auto base = new T_CB_Generic_NoArgs(DrawHudCB);
        base->Init();

        CHud::Components[aHudComponentInfo[HUD_RADAR].m_nIndex]->pos.x = 0.040f * HUD_SCALE;
    }

    static void DrawRadarBack() {
        float off = 0.1f;
        CSprite2d::DrawCircle({ 0.5f, 0.5f }, { 0.5f - off, 0.5f - off }, 40, { 5, 5, 5, 225 }, 0.0f);
    }

    static void DrawRadarDisc() {
        float off = 0.05f;
        spriteLoader.GetSprite("radardisc").SetRenderState();
        CSprite2d::Draw({ 0.0f + off, 1.0f - off + 0.01f }, { 0.0f + off, 0.0f + off + 0.01f }, { 1.0f - off, 1.0f - off + 0.01f }, { 1.0f - off, 0.0f + off + 0.01f }, elements[HUDELEMENT_RADAR_DISC].extracolor);
        CSprite2d::Draw({ 0.0f + off, 1.0f - off }, { 0.0f + off, 0.0f + off }, { 1.0f - off, 1.0f - off }, { 1.0f - off, 0.0f + off }, elements[HUDELEMENT_RADAR_DISC].color);
        CSprite2d::ClearRenderState();
    }

    static void DrawRadarMask() {

    }

    static void DrawDebugStuff() {
        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetWrapx(0.0f, 1.0f);
        CFont::SetOrientation(ALIGN_RIGHT);
        CFont::SetEdge(1.0f);
        CFont::SetScale(0.2f, 0.2f);
        CFont::SetFontStyle(FONT_HELVETICA);
        CFont::SetColor({ 225, 225, 225, 255 });
        CFont::SetDropColor({ 0, 0, 0, 255 });

        char buf[128] = { '\0' };
        wchar_t wbuf[128] = { '\0' };

        auto playa = FindPlayerPed(0);
        if (playa) {
            auto mat = playa->m_pMatrix;

            if (mat) {
                sprintf(buf, "%3.3f, %3.3f, %3.3f", mat->pos.x, mat->pos.y, mat->pos.z);
                AsciiToUnicode(buf, wbuf);
                CFont::PrintString(0.0f, 0.0f, wbuf);

                sprintf(buf, "%f", playa->GetForward().Magnitude());
                AsciiToUnicode(buf, wbuf);
                CFont::PrintString(0.0f, 0.025f, wbuf);

                CFont::DrawFonts();
            }
        }
    }


    static void Init() {
        spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("ClassicHud\\fonts"));
        spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("ClassicHud\\hud"));
        spriteLoader.LoadAllSpritesFromFolder(PLUGIN_PATH("ClassicHud\\weapons"));

        LoadFontData(PLUGIN_PATH("ClassicHud\\data\\fonts.dat"));
        LoadHudDetails(PLUGIN_PATH("ClassicHud\\data\\hud.dat"));

		LoadSettings();
    }

    static void Shutdown() {
        spriteLoader.Clear();
    }

    ClassicHudIV() {
#ifdef DEBUG
        plugin::OpenConsole();
#endif

        if (DebugMenuLoad()) {
            DebugMenuShowing = (bool(*)())GetProcAddress(gDebugMenuAPI.module, "DebugMenuShowing");
            DebugMenuPrintString = (void(*)(const char*, float, float, int))GetProcAddress(gDebugMenuAPI.module, "DebugMenuPrintString");
            DebugMenuGetStringSize = (int(*)(const char*))GetProcAddress(gDebugMenuAPI.module, "DebugMenuGetStringSize");
        }

        plugin::Events::initEngineEvent += []() {
            Init();
            DebugMenuAddCmd("ClassicHud", "Reload", []() {
                Shutdown();
                Init();
            });
        };

        plugin::Events::shutdownEngineEvent += []() {
            Shutdown();
        };

        static uint8_t rendered = 0;
        plugin::Events::drawHudEvent += []() {
            if (rendered) {
                rendered = 0;
                return;
            }

            DrawHud();
            rendered = 1;
        };

        // No zone name
        plugin::patch::SetUChar(plugin::pattern::Get("6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 80 3D ? ? ? ? ? A3 ? ? ? ? 74 ? 80 3D ? ? ? ? ? 75 ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? EB ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? FF 35 ? ? ? ? FF 35 ? ? ? ? 6A ? 68 ? ? ? ? 68 ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4", 1), -1);

        // No street name
        plugin::patch::SetUChar(plugin::pattern::Get("6A ? 68 ? ? ? ? E8 ? ? ? ? 83 C4 ? 80 3D ? ? ? ? ? A3 ? ? ? ? 74 ? 80 3D ? ? ? ? ? 75 ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? EB ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? FF D7", 1), -1);

        // No vehicle name
        plugin::patch::SetUChar(plugin::pattern::Get("6A ? 68 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? FF 35 ? ? ? ? FF 35 ? ? ? ? 6A ? 68 ? ? ? ? 68 ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? FF 35", 1), -1);

        // No weapon icon
        plugin::patch::SetUChar(plugin::pattern::Get("6A 02 68 ? ? ? ? E8 ? ? ? ? 8B 3D", 1), -1);

        // No ammo
        plugin::patch::SetUChar(plugin::pattern::Get("6A ? 68 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? A1 ? ? ? ? 8B 04 85 ? ? ? ? C6 40 ? ? A1 ? ? ? ? 8B 04 85 ? ? ? ? C6 40 ? ? FF 35 ? ? ? ? FF 35 ? ? ? ? 6A ? 68 ? ? ? ? 68 ? ? ? ? 6A ? 68 ? ? ? ? E8 ? ? ? ? 8B 3D ", 1), -1);

        // No money
        plugin::patch::SetUChar(plugin::pattern::Get("6A 05 68 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? 8B 04 85 ? ? ? ? 83 C4 54", 1), -1);

        // No wanted
        plugin::patch::SetUChar(plugin::pattern::Get("6A 05 68 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? 8B 04 85 ? ? ? ? C7 40 ? ? ? ? ? A1 ? ? ? ? 8B 04 85 ? ? ? ? C6 40 04 01 A1 ? ? ? ? 8B 04 85 ? ? ? ? C6 40 38 00 FF 35 ? ? ? ? FF 35 ? ? ? ? 6A 01 68 ? ? ? ? 68 ? ? ? ? 6A 05", 1), -1);
        plugin::patch::SetUChar(plugin::pattern::Get("6A 05 68 ? ? ? ? E8 ? ? ? ? A3 ? ? ? ? 8B 04 85 ? ? ? ? B9", 1), -1);

        plugin::patch::PutRetn(plugin::pattern::Get("83 EC 10 53 6A 00 E8")); // CRadar::DrawHealthAndArmour
        plugin::patch::RedirectJump(plugin::pattern::Get("83 EC 2C A1 ? ? ? ? 33 C4 89 44 24 28 83 3D"), DrawRadarBack);
        plugin::patch::RedirectJump(plugin::pattern::Get("83 EC 2C A1 ? ? ? ? 33 C4 89 44 24 28 C7 04 24 ? ? ? ? 8B 04 24 89 44 24 08 C7 44 24 ? ? ? ? ? 8B 44 24 04 89 44 24 0C C7 04 24 ? ? ? ? 8B 04 24 89 44 24 10 C7 44 24 ? ? ? ? ? 8B 44 24 04 89 44 24 14 C7 04 24 ? ? ? ? 8B 04 24 89 44 24 18"), DrawRadarDisc);

        // No radar alpha
        //plugin::patch::SetUChar(0x903774 + 1, 6);
    }
} classicHudIV;
