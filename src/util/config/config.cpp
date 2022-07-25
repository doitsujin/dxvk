#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <utility>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

namespace dxvk {

  const static std::vector<std::pair<const char*, Config>> g_appDefaults = {{
    /* Assassin's Creed Syndicate: amdags issues  */
    { R"(\\ACS\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Dissidia Final Fantasy NT Free Edition */
    { R"(\\dffnt\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Elite Dangerous: Compiles weird shaders    *
     * when running on AMD hardware               */
    { R"(\\EliteDangerous64\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Vanishing of Ethan Carter Redux        */
    { R"(\\EthanCarter-Win64-Shipping\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Evil Within: Submits command lists     * 
     * multiple times                             */
    { R"(\\EvilWithin(Demo)?\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* Far Cry 3: Assumes clear(0.5) on an UNORM  *
     * format to result in 128 on AMD and 127 on  *
     * Nvidia. We assume that the Vulkan drivers  *
     * match the clear behaviour of D3D11.        */
    { R"(\\(farcry3|fc3_blooddragon)_d3d11\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry 4: Same as Far Cry 3               */
    { R"(\\FarCry4\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Frostpunk: Renders one frame with D3D9     *
     * after creating the DXGI swap chain         */
    { R"(\\Frostpunk\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
      { "d3d11.cachedDynamicResources",     "c" },
    }} },
    /* Nioh: See Frostpunk, apparently?           */
    { R"(\\nioh\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Quantum Break: Mever initializes shared    *
     * memory in one of its compute shaders.      *
     * Also loves using MAP_WRITE on the same     *
     * set of resources multiple times per frame. */
    { R"(\\QuantumBreak\.exe$)", {{
      { "d3d11.zeroInitWorkgroupMemory",    "True" },
      { "d3d11.maxImplicitDiscardSize",     "-1"   },
    }} },
    /* Anno 2205: Random crashes with state cache */
    { R"(\\anno2205\.exe$)", {{
      { "dxvk.enableStateCache",            "False" },
    }} },
    /* Anno 1800: Poor performance without this   */
    { R"(\\Anno1800\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "c"    },
    }} },
    /* Fifa '19+: Binds typed buffer SRV to shader *
     * that expects raw/structured buffer SRV     */
    { R"(\\FIFA(19|[2-9][0-9])(_demo)?\.exe$)", {{
      { "dxvk.useRawSsbo",                  "True" },
    }} },
    /* Resident Evil 2/3: Ignore WaW hazards      */
    { R"(\\re(2|3|3demo)\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Devil May Cry 5                            */
    { R"(\\DevilMayCry5\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Call of Duty WW2                           */
    { R"(\\s2_sp64_ship\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Need for Speed 2015                        */
    { R"(\\NFS16\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mass Effect Andromeda                      */
    { R"(\\MassEffectAndromeda\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mirror`s Edge Catalyst: Crashes on AMD     */
    { R"(\\MirrorsEdgeCatalyst(Trial)?\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Star Wars Battlefront (2015)               */
    { R"(\\starwarsbattlefront(trial)?\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Dark Souls Remastered                      */
    { R"(\\DarkSoulsRemastered\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Grim Dawn                                  */
    { R"(\\Grim Dawn\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* NieR:Automata                              */
    { R"(\\NieRAutomata\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* NieR Replicant                             */
    { R"(\\NieR Replicant ver\.1\.22474487139\.exe)", {{
      { "dxgi.syncInterval",                "1"    },
      { "dxgi.maxFrameRate",                "60"   },
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* SteamVR performance test                   */
    { R"(\\vr\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Hitman 2 and 3 - requires AGS library      */
    { R"(\\HITMAN(2|3)\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare Remastered                  */
    { R"(\\h1_[ms]p64_ship\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Titan Quest                                */
    { R"(\\TQ\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Saints Row IV                              */
    { R"(\\SaintsRowIV\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Saints Row: The Third                      */
    { R"(\\SaintsRowTheThird_DX11\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Crysis 3 - slower if it notices AMD card     *
     * Apitrace mode helps massively in cpu bound   *
     * game parts                                   */
    { R"(\\Crysis3\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Crysis 3 Remastered                          *
     * Apitrace mode helps massively in cpu bound   *
     * game parts                                   */
    { R"(\\Crysis3Remastered\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Atelier series - games try to render video *
     * with a D3D9 swap chain over the DXGI swap  *
     * chain, which breaks D3D11 presentation     */
    { R"(\\Atelier_(Ayesha|Escha_and_Logy|Shallie)(_EN)?\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Atelier Firis                              */
    { R"(\\A18\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Atelier Rorona/Totori/Meruru               */
    { R"(\\A(11R|12V|13V)_x64_Release(_en)?\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Just how many of these games are there?    */
    { R"(\\Atelier_(Lulua|Lydie_and_Suelle|Ryza(_2)?|Sophie_2)\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* ...                                        */
    { R"(\\Atelier_(Lydie_and_Suelle|Firis|Sophie)_DX\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Fairy Tail                                 */
    { R"(\\FAIRY_TAIL\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Nights of Azure                            */
    { R"(\\CNN\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Star Wars Battlefront II: amdags issues    */
    { R"(\\starwarsbattlefrontii\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* F1 games - do not synchronize TGSM access  *
     * in a compute shader, causing artifacts     */
    { R"(\\F1_20(1[89]|[2-9][0-9])\.exe$)", {{
      { "d3d11.forceTgsmBarriers",          "True" },
    }} },
    /* Blue Reflection                            */
    { R"(\\BLUE_REFLECTION\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Secret World Legends                       */
    { R"(\\SecretWorldLegendsDX11\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Darksiders Warmastered - apparently reads  *
     * from write-only mapped buffers             */
    { R"(\\darksiders1\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Monster Hunter World                       */
    { R"(\\MonsterHunterWorld\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Kingdome Come: Deliverance                 */
    { R"(\\KingdomCome\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Homefront: The Revolution                  */
    { R"(\\Homefront2_Release\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Sniper Ghost Warrior Contracts             */
    { R"(\\SGWContracts\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Armored Warfare             */
    { R"(\\armoredwarfare\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "c"    },
    }} },
    /* Shadow of the Tomb Raider - invariant      *
     * position breaks character rendering on NV  */
    { R"(\\SOTTR\.exe$)", {{
      { "d3d11.invariantPosition",          "False" },
      { "d3d11.floatControls",              "False" },
    }} },
    /* Nioh 2                                     */
    { R"(\\nioh2\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* DIRT 5 - uses amd_ags_x64.dll when it      *
     * detects an AMD GPU                         */
    { R"(\\DIRT5\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Crazy Machines 3 - crashes on long device  *
     * descriptions                               */
    { R"(\\cm3\.exe$)", {{
      { "dxgi.customDeviceDesc",            "DXVK Adapter" },
    }} },
    /* World of Final Fantasy: Broken and useless *
     * use of 4x MSAA throughout the renderer     */
    { R"(\\WOFF\.exe$)", {{
      { "d3d11.disableMsaa",                "True" },
    }} },
    /* Final Fantasy XIV - Stuttering on NV       */
    { R"(\\ffxiv_dx11\.exe$)", {{
      { "dxvk.shrinkNvidiaHvvHeap",         "True" },
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* God of War - relies on NVAPI/AMDAGS for    *
     * barrier stuff, needs nvapi for DLSS        */
    { R"(\\GoW\.exe$)", {{
      { "d3d11.ignoreGraphicsBarriers",     "True" },
      { "d3d11.relaxedBarriers",            "True" },
      { "dxgi.nvapiHack",                   "False" },
      { "dxgi.maxFrameLatency",             "1"    },
    }} },
    /* AoE 2 DE - runs poorly for some users      */
    { R"(\\AoE2DE_s\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Total War: Warhammer III                   */
    { R"(\\Warhammer3\.exe$)", {{
      { "d3d11.maxDynamicImageBufferSize",  "4096" },
    }} },
    /* Assassin's Creed 3 and 4                   */
    { R"(\\ac(3|4bf)[sm]p\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Stranger of Paradise - FF Origin           */
    { R"(\\SOPFFO\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    /* Small Radios Big Televisions               */
    }} },
    { R"(\\SRBT\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* A Way Out: fix for stuttering and low fps  */
    { R"(\\AWayOut(_friend)?\.exe$)", {{
      { "dxgi.maxFrameLatency",                "1" },
    }} },
    /* Garden Warfare 2
       Won't start on amd Id without atiadlxx     */
    { R"(\\GW2.Main_Win64_Retail\.exe$)", {{
      { "dxgi.customVendorId",           "10de"   },
    }} },
    /* DayZ */
    { R"(\\DayZ_x64\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "cr" },
    }} },
    /* Stray - writes to the same UAV every draw, *
     * presumably for culling, which doesn't play *
     * nicely with D3D11 without vendor libraries */
    { R"(\\Stray-Win64-Shipping\.exe$)", {{
      { "d3d11.ignoreGraphicsBarriers",     "True" },
    }} },

    /**********************************************/
    /* D3D9 GAMES                                 */
    /**********************************************/

    /* A Hat in Time                              */
    { R"(\\HatinTimeGame\.exe$)", {{
      { "d3d9.strictPow",                   "False" },
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Anarchy Online                             */
    { R"(\\anarchyonline\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* Borderlands 2 and The Pre Sequel!           */
    { R"(\\Borderlands(2|PreSequel)\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
      { "d3d9.supportDFFormats",            "False" },
    }} },
    /* Borderlands                                */
    { R"(\\Borderlands\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Gothic 3                                   */
    { R"(\\Gothic(3|3Final| III Forsaken Gods)\.exe$)", {{
      { "d3d9.supportDFFormats",            "False" },
    }} },
    /* Sonic Adventure 2                          */
    { R"(\\Sonic Adventure 2\\(launcher|sonic2app)\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* The Sims 2,
       Body Shop,
       The Sims Life Stories,
       The Sims Pet Stories,
       and The Sims Castaway Stories             */
    { R"(\\(Sims2.*|TS2BodyShop|SimsLS|SimsPS|SimsCS)\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "0091" },
      { "d3d9.customDeviceDesc",            "GeForce 7800 GTX" },
      { "d3d9.disableA8RT",                 "True" },
      { "d3d9.supportX4R4G4B4",             "False" },
      { "d3d9.maxAvailableMemory",          "2048" },
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* Dead Space uses the a NULL render target instead
       of a 1x1 one if DF24 is NOT supported     
       Mouse and physics issues above 60 FPS
       Built-in Vsync Locks the game to 30 FPS    */
    { R"(\\Dead Space\.exe$)", {{
      { "d3d9.supportDFFormats",                 "False" },
      { "d3d9.maxFrameRate",                     "60" },
      { "d3d9.presentInterval",                  "1" },
    }} },
    /* Halo CE/HaloPC                             */
    { R"(\\halo(ce)?\.exe$)", {{
      // Game enables minor decal layering fixes
      // specifically when it detects AMD.
      // Avoids chip being detected as unsupported
      // when on intel. Avoids possible path towards
      // invalid texture addressing methods.
      { "d3d9.customVendorId",              "1002" },
      // Avoids card not recognized error.
      // Keeps game's rendering methods consistent
      // for optimal compatibility.
      { "d3d9.customDeviceId",              "4172" },
      // The game uses incorrect sampler types in
      // the shaders for glass rendering which
      // breaks it on native + us if we don't
      // spec-constantly chose the sampler type
      // automagically.
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Counter Strike: Global Offensive
       Needs NVAPI to avoid a forced AO + Smoke
       exploit so we must force AMD vendor ID.    */
    { R"(\\csgo\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* Vampire - The Masquerade Bloodlines        */
    { R"(\\vampire\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "1024" },
    }} },
    /* Senran Kagura Shinovi Versus               */
    { R"(\\SKShinoviVersus\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* Metal Slug X                               */
    { R"(\\mslugx\.exe$)", {{
      { "d3d9.supportD32",                  "False" },
    }} },
    /* Skyrim (NVAPI)                             */
    { R"(\\TESV\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* RTHDRIBL Demo                              
       Uses DONOTWAIT after GetRenderTargetData
       then goes into an infinite loop if it gets
       D3DERR_WASSTILLDRAWING.
       This is a better solution than penalizing
       other apps that use this properly.         */
    { R"(\\rthdribl\.exe$)", {{
      { "d3d9.allowDoNotWait",              "False" },
    }} },
    /* Hyperdimension Neptunia U: Action Unleashed */
    { R"(\\Neptunia\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* D&D - The Temple Of Elemental Evil          */
    { R"(\\ToEE\.exe$)", {{
      { "d3d9.allowDiscard",                "False" },
    }} },
    /* ZUSI 3 - Aerosoft Edition                  */
    { R"(\\ZusiSim\.exe$)", {{
      { "d3d9.noExplicitFrontBuffer",       "True" },
    }} },
    /* GTA IV (NVAPI)                             */
    /* Also thinks we're always on Intel          *
     * and will report/use bad amounts of VRAM.   */
    { R"(\\GTAIV\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
      { "dxgi.emulateUMA",                  "True" },
    }} },
    /* Battlefield 2 (bad z-pass)                 */
    { R"(\\BF2\.exe$)", {{
      { "d3d9.longMad",                     "True" },
    }} },
    /* SpellForce 2 Series                        */
    { R"(\\SpellForce2.*\.exe$)", {{
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Everquest 2                                */
    { R"(\\EverQuest2.*\.exe$)", {{
      { "d3d9.alphaTestWiggleRoom", "True" },
    }} },
    /* Tomb Raider: Legend                       */
    { R"(\\trl\.exe$)", {{
      { "d3d9.apitraceMode",                "True" },
    }} },
    /* Everquest                                 */
    { R"(\\eqgame\.exe$)", {{
      { "d3d9.apitraceMode",                "True" },
    }} },
    /* Dark Messiah of Might & Magic             */
    { R"(\\mm\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* Mafia 2                                   */
    { R"(\\mafia2\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "0402" },
    }} },
    /* Warhammer: Online                         */
    { R"(\\WAR(-64)?\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
    }} },
    /* Dragon Nest                               */
    { R"(\\DragonNest_x64\.exe$)", {{
      { "d3d9.memoryTrackTest ",            "True" },
    }} },
    /* Dal Segno                                 */
    { R"(\\DST\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Kohan II                                  */
    { R"(\\k2\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* Ninja Gaiden Sigma 1/2                    */
    { R"(\\NINJA GAIDEN SIGMA(2)?\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Demon Stone breaks at frame rates > 60fps */
    { R"(\\Demonstone\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Far Cry 1 has worse water rendering when it detects AMD GPUs */
    { R"(\\FarCry\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
    }} },
    /* Earth Defense Force 5 */
    { R"(\\EDF5\.exe$)", {{
      { "dxgi.tearFree",                    "False" },
      { "dxgi.syncInterval",                "1"     },
    }} },
    /* Sine Mora EX */
    { R"(\\SineMoraEX\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Fantasy Grounds                           */
    { R"(\\FantasyGrounds\.exe$)", {{
      { "d3d9.noExplicitFrontBuffer",       "True" },
    }} },
    /* Red Orchestra 2                           */
    { R"(\\ROGame\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Dark Souls II                            */
    { R"(\\DarkSoulsII\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Dogfight 1942                            */
    { R"(\\Dogfight1942\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Bayonetta                                */
    { R"(\\Bayonetta\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Rayman Origins                           */
    { R"(\\Rayman Origins\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Guilty Gear Xrd -Relevator-              */
    { R"(\\GuiltyGearXrd\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Richard Burns Rally                      */
    { R"(\\RichardBurnsRally_SSE\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* BlazBlue Centralfiction                  */
    { R"(\\BBCF\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Resident Evil games                      */
    { R"(\\(rerev|rerev2|re0hd|bhd|re5dx9|BH6)\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",                "False" },
    }} },
    /* Limbo                                    */
    { R"(\\limbo\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Warhammer: Return of Reckoning Launcher
       Forcing SM1 fixes a black window otherwise caused by
       the lack of support for partial presentation */
    { R"(\\RoRLauncher\.exe$)", {{
      { "d3d9.shaderModel",                 "1" },
    }} },
    /* Halo CE SPV3 launcher
       Same issue as Warhammer: RoR above       */
    { R"(\\spv3\.exe$)", {{
      { "d3d9.shaderModel",                 "1" },
    }} },
    /* Star Wars The Force Unleashed 2          *
     * Black particles because it tries to bind *
     * a 2D texture for a shader that           *
     * declares a 3d texture.                   */
    { R"(\\SWTFU2\.exe$)", {{
      { "d3d9.forceSamplerTypeSpecConstants",  "True" },
    }} },
    /* Scrapland (Remastered)                   */
    { R"(\\Scrap\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Majesty 2 (Collection)                   *
     * Crashes on UMA without a memory limit,   *
     * since the game(s) will allocate all      *
     * available VRAM on startup.               */
    { R"(\\Majesty2\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "2048" },
    }} },
    /* Myst V End of Ages                             
       Game has white textures on amd radv.
       Expects Nvidia, Intel or ATI VendorId.
       "Radeon" in gpu description also works   */
    { R"(\\eoa\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
    }} },
    /* Beyond Good And Evil                     *
     * Fixes missing sun and light shafts       */
    { R"(\\BGE\.exe$)", {{
      { "d3d9.allowDoNotWait",              "False" },
    }} },
    /* Supreme Commander & Forged Alliance Forever */
    { R"(\\(SupremeCommander|ForgedAlliance)\.exe$)", {{
      { "d3d9.floatEmulation",            "Strict" },
    }} },
    /* Star Wars The Old Republic */
    { R"(\\swtor\.exe$)", {{
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Bionic Commando                          
       Physics break at high fps               */
    { R"(\\bionic_commando\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Port Royale 3                            *
     * Fixes infinite loading screens           */
    { R"(\\PortRoyale3\.exe$)", {{
      { "d3d9.allowDoNotWait",           "False" },
    }} },
    /* Need For Speed 3 modern patch            */
    { R"(\\nfs3\.exe$)", {{
      { "d3d9.enableDialogMode",          "True" },
    }} },
    /* Ninja Blade                              *
     * Transparent main character on Nvidia     */
    { R"(\\NinjaBlade\.exe$)", {{
      { "d3d9.alphaTestWiggleRoom",       "True" },
    }} },
    /* YS Origin                                *
     * Helps very bad frametimes in some areas  */
    { R"(\\yso_win\.exe$)", {{
      { "d3d9.maxFrameLatency",              "1" },
    }} },
  }};


  static bool isWhitespace(char ch) {
    return ch == ' ' || ch == '\x9' || ch == '\r';
  }

  
  static bool isValidKeyChar(char ch) {
    return (ch >= '0' && ch <= '9')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch == '.' || ch == '_');
  }


  static size_t skipWhitespace(const std::string& line, size_t n) {
    while (n < line.size() && isWhitespace(line[n]))
      n += 1;
    return n;
  }


  struct ConfigContext {
    bool active;
  };


  static void parseUserConfigLine(Config& config, ConfigContext& ctx, const std::string& line) {
    std::stringstream key;
    std::stringstream value;

    // Extract the key
    size_t n = skipWhitespace(line, 0);

    if (n < line.size() && line[n] == '[') {
      n += 1;

      size_t e = line.size() - 1;
      while (e > n && line[e] != ']')
        e -= 1;

      while (n < e)
        key << line[n++];
      
      ctx.active = key.str() == env::getExeName();
    } else {
      while (n < line.size() && isValidKeyChar(line[n]))
        key << line[n++];
      
      // Check whether the next char is a '='
      n = skipWhitespace(line, n);
      if (n >= line.size() || line[n] != '=')
        return;

      // Extract the value
      bool insideString = false;
      n = skipWhitespace(line, n + 1);

      while (n < line.size()) {
        if (!insideString && isWhitespace(line[n]))
          break;

        if (line[n] == '"') {
          insideString = !insideString;
          n++;
        } else
          value << line[n++];
      }
      
      if (ctx.active)
        config.setOption(key.str(), value.str());
    }
  }


  Config::Config() { }
  Config::~Config() { }


  Config::Config(OptionMap&& options)
  : m_options(std::move(options)) { }


  void Config::merge(const Config& other) {
    for (auto& pair : other.m_options)
      m_options.insert(pair);
  }


  void Config::setOption(const std::string& key, const std::string& value) {
    m_options.insert_or_assign(key, value);
  }


  std::string Config::getOptionValue(const char* option) const {
    auto iter = m_options.find(option);

    return iter != m_options.end()
      ? iter->second : std::string();
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          std::string&  result) {
    result = value;
    return true;
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          bool&         result) {
    static const std::array<std::pair<const char*, bool>, 2> s_lookup = {{
      { "true",  true  },
      { "false", false },
    }};

    return parseStringOption(value,
      s_lookup.begin(), s_lookup.end(), result);
  }


  bool Config::parseOptionValue(
    const std::string&  value,
          int32_t&      result) {
    if (value.size() == 0)
      return false;
    
    // Parse sign, don't allow '+'
    int32_t sign = 1;
    size_t start = 0;

    if (value[0] == '-') {
      sign = -1;
      start = 1;
    }

    // Parse absolute number
    int32_t intval = 0;

    for (size_t i = start; i < value.size(); i++) {
      if (value[i] < '0' || value[i] > '9')
        return false;
      
      intval *= 10;
      intval += value[i] - '0';
    }

    // Apply sign and return
    result = sign * intval;
    return true;
  }
  
  
  bool Config::parseOptionValue(
    const std::string&  value,
          Tristate&     result) {
    static const std::array<std::pair<const char*, Tristate>, 3> s_lookup = {{
      { "true",  Tristate::True  },
      { "false", Tristate::False },
      { "auto",  Tristate::Auto  },
    }};

    return parseStringOption(value,
      s_lookup.begin(), s_lookup.end(), result);
  }


  template<typename I, typename V>
  bool Config::parseStringOption(
          std::string   str,
          I             begin,
          I             end,
          V&            value) {
    str = Config::toLower(str);

    for (auto i = begin; i != end; i++) {
      if (str == i->first) {
        value = i->second;
        return true;
      }
    }

    return false;
  }


  Config Config::getAppConfig(const std::string& appName) {
    auto appConfig = std::find_if(g_appDefaults.begin(), g_appDefaults.end(),
      [&appName] (const std::pair<const char*, Config>& pair) {
        std::regex expr(pair.first, std::regex::extended | std::regex::icase);
        return std::regex_search(appName, expr);
      });
    
    if (appConfig != g_appDefaults.end()) {
      // Inform the user that we loaded a default config
      Logger::info(str::format("Found built-in config:"));

      for (auto& pair : appConfig->second.m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));

      return appConfig->second;
    }

    return Config();
  }


  Config Config::getUserConfig() {
    Config config;

    // Load either $DXVK_CONFIG_FILE or $PWD/dxvk.conf
    std::string filePath = env::getEnvVar("DXVK_CONFIG_FILE");

    if (filePath == "")
      filePath = "dxvk.conf";
    
    // Open the file if it exists
    std::ifstream stream(str::tows(filePath.c_str()).c_str());

    if (!stream)
      return config;
    
    // Inform the user that we loaded a file, might
    // help when debugging configuration issues
    Logger::info(str::format("Found config file: ", filePath));

    // Initialize parser context
    ConfigContext ctx;
    ctx.active = true;

    // Parse the file line by line
    std::string line;

    while (std::getline(stream, line))
      parseUserConfigLine(config, ctx, line);
    
    return config;
  }


  void Config::logOptions() const {
    if (!m_options.empty()) {
      Logger::info("Effective configuration:");

      for (auto& pair : m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));
    }
  }

  std::string Config::toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
      [] (unsigned char c) { return (c >= 'A' && c <= 'Z') ? (c + 'a' - 'A') : c; });
    return str;
  }

}
