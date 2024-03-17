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
     /* EVE Online: Needs this to expose D3D12     *
     * otherwise D3D12 option on launcher is      *
     * greyed out                                 */
    { R"(\\evelauncher\.exe$)", {{
      { "d3d11.maxFeatureLevel",            "12_1" },
    }} },
    /* The Evil Within: Submits command lists     *
     * multiple times                             */
    { R"(\\EvilWithin(Demo)?\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* Far Cry 2: Set vendor ID to Nvidia to avoid
     * vegetation artifacts on Intel, and set
     * apitrace mode to True to improve perf on all
     * hardware.                                  */
    { R"(\\(FarCry2|farcry2game)\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Far Cry 3: Assumes clear(0.5) on an UNORM  *
     * format to result in 128 on AMD and 127 on  *
     * Nvidia. We assume that the Vulkan drivers  *
     * match the clear behaviour of D3D11.        *
     * Intel needs to match the AMD result        */
    { R"(\\(farcry3|fc3_blooddragon)_d3d11\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
      { "dxgi.hideIntelGpu",                "True" },
    }} },
    /* Far Cry 4 and Primal: Same as Far Cry 3    */
    { R"(\\(FarCry4|FCPrimal)\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
      { "dxgi.hideIntelGpu",                "True" },
    }} },
    /* Frostpunk: Renders one frame with D3D9     *
     * after creating the DXGI swap chain         */
    { R"(\\Frostpunk\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
      { "d3d11.cachedDynamicResources",     "c" },
    }} },
    /* Nioh: Apparently the same as the Atelier games  */
    { R"(\\nioh\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
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
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Need for Speed 2015                        */
    { R"(\\NFS16\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Mass Effect Andromeda                      */
    { R"(\\MassEffectAndromeda\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Mirror`s Edge Catalyst: Crashes on AMD     */
    { R"(\\MirrorsEdgeCatalyst(Trial)?\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Star Wars Battlefront (2015)               */
    { R"(\\starwarsbattlefront(trial)?\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
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
    /* Hitman 2 - requires AGS library      */
    { R"(\\HITMAN2\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare Remastered                  */
    { R"(\\h1(_[ms]p64_ship|-mod)\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare 2 Campaign Remastered       *
     * AMD AGS crash same as above                */
    { R"(\\MW2CR\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
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
    { R"(\\Atelier_(Lulua|Lydie_and_Suelle|Ryza(_2|_3)?|Sophie_2)\.exe$)", {{
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
      { "d3d11.forceVolatileTgsmAccess",    "True" },
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
     /* Mary Skelter 2 - Broken MSAA              */
    { R"(\\MarySkelter2\.exe$)", {{
      { "d3d11.disableMsaa",                "True" },
    }} },
    /* Final Fantasy XIV: Uses lots of HVV and    *
     * also reads some uncached memory.           */
    { R"(\\ffxiv_dx11\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* Final Fantasy XV: VXAO does thousands of   *
     * draw calls with the same UAV bound         */
    { R"(\\ffxv_s\.exe$)", {{
      { "d3d11.ignoreGraphicsBarriers",     "True" },
    }} },
    /* God of War - relies on NVAPI/AMDAGS for    *
     * barrier stuff, needs nvapi for DLSS        */
    { R"(\\GoW\.exe$)", {{
      { "d3d11.ignoreGraphicsBarriers",     "True" },
      { "d3d11.relaxedBarriers",            "True" },
      { "dxgi.hideNvidiaGpu",               "False" },
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
      { "dxgi.maxFrameLatency",             "1" },
    }} },
    /* Garden Warfare 2
       Won't start on amd Id without atiadlxx     */
    { R"(\\GW2\.Main_Win64_Retail\.exe$)", {{
      { "dxgi.customVendorId",              "10de"   },
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
    /* Metal Gear Solid V: Ground Zeroes          *
     * Texture quality can break at high vram     */
    { R"(\\MgsGroundZeroes\.exe$)", {{
      { "dxgi.maxDeviceMemory",             "4095" },
    }} },
    /* Shantae and the Pirate's Curse             *
     * Game speeds up above 60 fps                */
    { R"(\\ShantaeCurse\.exe$)", {{
      { "dxgi.maxFrameRate",                "60" },
    }} },
    /* Mighty Switch Force! Collection            *
     * Games speed up above 60 fps                */
    { R"(\\MSFC\.exe$)", {{
      { "dxgi.maxFrameRate",                "60" },
    }} },
    /* Cardfight!! Vanguard Dear Days:            *
     * Submits command lists multiple times       */
    { R"(\\VG2\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Battlefield: Bad Company 2                 *
     * Gets rid of black flickering               */
    { R"(\\BFBC2Game\.exe$)", {{
      { "d3d11.floatControls",              "False" },
    }} },
    /* Sonic Frontiers - flickering shadows and   *
     * vegetation when GPU-bound                  */
    { R"(\\SonicFrontiers\.exe$)", {{
      { "dxgi.maxFrameLatency",             "1" },
    }} },
    /* TRAHA Global                               *
     * Shadow issues when it sees AMD/Nvidia      */
    { R"(\\RapaNui-Win64-Shipping\.exe$)", {{
      { "dxgi.customVendorId",              "8086" },
    }} },
    /* SpellForce 3 Reforced & expansions         *
     * Greatly improves CPU bound performance     */
    { R"(\\SF3ClientFinal\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "v" },
    }} },
    /* Tom Clancy's Ghost Recon Breakpoint        */
    { R"(\\GRB\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* GTA V performance issues                   */
    { R"(\\GTA5\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* Crash Bandicoot N. Sane Trilogy            *
     * Work around some vsync funkiness           */
    { R"(\\CrashBandicootNSaneTrilogy\.exe$)", {{
      { "dxgi.syncInterval",                "1"   },
    }} },
    /* SnowRunner                                 */
    { R"(\\SnowRunner\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Rockstar Games Launcher                    */
    { R"(\\Rockstar Games\\Launcher\\Launcher\.exe$)", {{
      { "dxvk.maxChunkSize",                "1"   },
    }} },
    /* Rockstar Social Club                       */
    { R"(\\Rockstar Games\\Social Club\\SocialClubHelper\.exe$)", {{
      { "dxvk.maxChunkSize",                "1"   },
    }} },
    /* EA Desktop App                             */
    { R"(\\EADesktop\.exe$)", {{
      { "dxvk.maxChunkSize",                "1"   },
    }} },
    /* GOG Galaxy                                 */
    { R"(\\GalaxyClient\.exe$)", {{
      { "dxvk.maxChunkSize",                "1"   },
    }} },
    /* Fallout 76
     * Game tries to be too "smart" and changes sync
     * interval based on performance (in fullscreen)
     * or tries to match (or ratio below) 60fps
     * (in windowed).
     *
     * Ends up getting in a loop where it will switch
     * and start stuttering, or get stuck at targeting
     * 30Hz in fullscreen.
     * Windowed mode being locked to 60fps as well is
     * pretty suboptimal...
     */
    { R"(\\Fallout76\.exe$)", {{
      { "dxgi.syncInterval",                "1" },
    }} },
    /* Blizzard Entertainment Battle.net          */
    { R"(\\Battle\.net\.exe$)", {{
      { "dxvk.maxChunkSize",                "1" },
    }} },
    /* Bladestorm Nightmare                       *
     * Game speed increases when above 60 fps in  *
     * the tavern area                            */
    { R"(\\BLADESTORM Nightmare\\Launch_(EA|JP)\.exe$)", {{
      { "dxgi.maxFrameRate",                "60"  },
    }} },
    /* Ghost Recon Wildlands                      */
    { R"(\\GRW\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Vindictus d3d11 CPU bound perf             */
    { R"(\\Vindictus(_x64)?\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "cr"   },
    }} },
    /* Riders Republic - Statically linked AMDAGS */
    { R"(\\RidersRepublic(_BE)?\.exe$)", {{
      { "dxgi.hideAmdGpu",                "True"   },
    }} },
    /* HoloCure - Save the Fans!
       Same as Cyberpunk 2077                     */
    { R"(\\HoloCure\.exe$)", {{
      { "dxgi.useMonitorFallback",          "True" },
    }} },
    /* Kenshi                                     *
     * Helps CPU bound performance                */
    { R"(\\kenshi_x64\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "v"    },
    }} },
    /* Granblue Relink: Spams pixel shader UAVs   *
     * and assumes that AMD GPUs do not expose    *
     * native command lists for AGS usage         */
    { R"(\\granblue_fantasy_relink\.exe$)", {{
      { "d3d11.ignoreGraphicsBarriers",     "True"  },
      { "d3d11.exposeDriverCommandLists",   "False" },
      { "dxgi.hideNvidiaGpu",               "False" },
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
      { "d3d9.maxFrameRate",                "60" },
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
    /* Hyperdimension Neptunia U: Action Unleashed */
    { R"(\\Neptunia\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* GTA IV (NVAPI)                             */
    /* Also thinks we're always on Intel          *
     * and will report/use bad amounts of VRAM.
     * Disabling support for DF texture formats
     * makes the game use a better looking render
     * path for mirrors.
     * Also runs into issues after alt-tabbing.   */
    { R"(\\(GTAIV|EFLC)\.exe$)", {{
      { "d3d9.customVendorId",              "1002" },
      { "dxgi.emulateUMA",                  "True" },
      { "d3d9.supportDFFormats",            "False" },
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Battlefield 2                              *
     * Bad z-pass and ingame GUI loss on alt tab  */
    { R"(\\BF2\.exe$)", {{
      { "d3d9.longMad",                     "True" },  
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Battlefield 2142 - Same GUI issue as BF2   */
    { R"(\\BF2142\.exe$)", {{ 
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* SpellForce 2 Series                        */
    { R"(\\SpellForce2.*\.exe$)", {{
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Tomb Raider: Legend, Anniversary, Underworld  *
     * Read from a buffer created with:              *
     * D3DPOOL_DEFAULT,                              *
     * D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY buffer  */
    { R"(\\(trl|tra|tru)\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Everquest                                 */
    { R"(\\eqgame\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
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
    { R"(\\(WAR(-64)?|WARTEST(-64)?)\.exe$)", {{
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
      { "d3d9.textureMemory",               "0"   },
    }} },
    /* Limbo                                    */
    { R"(\\limbo\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Escape from Tarkov launcher
       Same issue as Warhammer: RoR above       */
    { R"(\\BsgLauncher\.exe$)", {{
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
    /* Supreme Commander & Forged Alliance Forever */
    { R"(\\(SupremeCommander|ForgedAlliance)\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
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
    /* Need For Speed 3 modern patch            */
    { R"(\\nfs3\.exe$)", {{
      { "d3d9.enableDialogMode",            "True" },
    }} },
    /* Beyond Good And Evil                     *
     * UI breaks at high fps                     */
    { R"(\\BGE\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* King Of Fighters XIII                     *
     * In-game speed increases on high FPS       */
    { R"(\\kof(xiii|13_win32_Release)\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* YS Origin                                *
     * Helps very bad frametimes in some areas  */
    { R"(\\yso_win\.exe$)", {{
      { "d3d9.maxFrameLatency",             "1" },
    }} },
    /* Saints Row 2 - Prevents unmap crash      */
    { R"(\\SR2_pc\.exe$)", {{
      { "d3d9.textureMemory",               "0" },
    }} },
    /* Witcher 1: Very long loading times       */
    { R"(\\witcher\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Guitar Hero World Tour                   *
     * Very prone to address space crashes      */
    { R"(\\(GHWT|GHWT_Definitive)\.exe$)", {{
      { "d3d9.textureMemory",               "16" },
      { "d3d9.allowDirectBufferMapping",    "False" },
    }} },
    /* Heroes of Annihilated Empires            *
     * Has issues with texture rendering and    *
     * video memory detection otherwise.        */
    { R"(\\Heroes (o|O)f Annihilated Empires.*\\engine\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "2048" },
    }} },
    /* The Ship (2004)                          */
    { R"(\\ship\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* SiN Episodes Emergence                   */
    { R"(\\SinEpisodes\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* Hammer World Editor                      */
    { R"(\\(hammer(plusplus)?|mallet|wc)\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Dragon Age Origins                       *
     * Keeps unmapping the same 3 1MB buffers   *
     * thousands of times when you alt-tab out  *
     * Causing it to crash OOM                  */
    { R"(\\DAOrigins\.exe$)" , {{
      { "d3d9.allowDirectBufferMapping",    "False" },
    }} },
    /* Fallout 3 - Doesn't like Intel Id       */
    { R"(\\Fallout3\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
    }} },
    /* Sonic & All-Stars Racing Transformed    *
     * Helps performance when Resizable BAR    *
     * is enabled                              */
    { R"(\\ASN_App_PcDx9_Final\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Black Mesa                              *
     * Artifacts & broken flashlight on Intel  */
    { R"(\\bms\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
    }} },
    /* Final Fantasy XIV - Direct3D 9 mode     *
     * Can crash with unmapping                */
    { R"(\\ffxiv\.exe$)", {{
      { "d3d9.textureMemory",               "0"   },
    }} },
    /* Alien Rage                              *
     * GTX 295 & disable Hack to fix shadows   */
    { R"(\\(ShippingPC-AFEARGame|ARageMP)\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "05E0" },
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Battle Fantasia Revised Edition         *
     * Speedup above 60fps                     */
    { R"(\\bf10\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Codename Panzers Phase One/Two          *
     * Main menu won't render after intros     *
     * and CPU bound performance               */
    { R"(\\(PANZERS|PANZERS_Phase_2)\.exe$)", {{
      { "d3d9.enableDialogMode",            "True"   },
      { "d3d9.cachedDynamicBuffers",        "True"   },
    }} },
    /* DC Universe Online                      *
     * Freezes after alt tabbing               */
    { R"(\\DCGAME\.EXE$)", {{
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Halo Online                             *
     * Black textures                          */
    { R"(\\eldorado\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict"   },
      { "d3d9.allowDirectBufferMapping",    "False" },
    }} },
    /* Injustice: Gods Among Us                *
     * Locks a buffer that's still in use      */
    { R"(\\injustice\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",    "False" },
    }} },
    /* STEINS;GATE ELITE                       */
    { R"(\\SG_ELITE\\Game\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* The Incredibles                         */
    { R"(\\IncPC\.exe$)", {{
      { "d3d9.maxFrameRate",                "59" },
    }} },
    /* Conflict Vietnam                        */
    { R"(\\Vietnam\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Project: Snowblind                      */
    { R"(\\Snowblind\.(SP|MP|exe)$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Aviary Attorney                         */
    { R"(\\Aviary Attorney\\nw\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Drakensang: The Dark Eye                */
    { R"(\\drakensang\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Age of Empires 2 - janky frame timing   */
    { R"(\\AoK HD\.exe$)", {{
      { "d3d9.maxFrameLatency",             "1" },
    }} },
    /* Battlestations Midway                   */
    { R"(\\Battlestationsmidway\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",     "True" },
    }} },
    /* SkyDrift                                 *
     * Works around alt tab OOM crash           */
    { R"(\\SkyDrift\.exe$)" , {{
      { "d3d9.allowDirectBufferMapping",    "False" },
    }} },
     /* Assassin's Creed 2                      *
     *  Helps alt tab crash on Linux            */
    { R"(\\AssassinsCreedIIGame\.exe$)" , {{
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Sonic CD                                */
    { R"(\\soniccd\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* UK Truck Simulator 1                    */
    { R"(\\UK Truck Simulator\\bin\\win_x86\\game\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* d3d9 Supreme Ruler games              *
     * Leaks a StateBlock leading                *
     * to Reset calls failing                    */
    { R"(\\SupremeRuler(Ultimate|GreatWar|1936|CW)\.exe$)", {{
      { "d3d9.countLosableResources",       "False" },
    }} },
    /* Operation Flashpoint: Red River           *
     * Flickering issues                         */
    { R"(\\RedRiver\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Dark Void - Crashes above 60fps in places */
    { R"(\\ShippingPC-SkyGame\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },


    /**********************************************/
    /* D3D12 GAMES (vkd3d-proton with dxvk dxgi)  */
    /**********************************************/

    /* Diablo 4 - Will complain about missing  *
     * GPU unless dxgi Id match actual GPU Id  */
    { R"(\\Diablo IV\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False"  },
    }} },
    /* WILD HEARTS™️                            *
     * D3D12 title using D3D11 device for      *
     * media texture creation, whereby a large *
     * chunk size only slows down media        *
     * initialization                          */
    { R"(\\WILD HEARTS(_Trial)?\.exe$)", {{
      { "dxvk.maxChunkSize",                 "4" },
    }} },
    /* Ratchet & Clank: Rift Apart - does not allow
     * enabling ray tracing if it sees an AMD GPU. */
    { R"(\\RiftApart\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* CP2077 enumerates display outputs each frame.
     * Avoid using QueryDisplayConfig to avoid
     * performance degradation until the
     * optimization of that function is in Proton. */
    { R"(\\Cyberpunk2077\.exe$)", {{
      { "dxgi.useMonitorFallback",          "True" },
    }} },
    /* Metro Exodus Enhanced Edition picks GPU adapters
     * by available VRAM, which causes issues on some
     * systems with integrated graphics. */
    { R"(\\Metro Exodus Enhanced Edition\\MetroExodus\.exe$)", {{
      { "dxvk.hideIntegratedGraphics",      "True" },
    }} },
    /* Persona 3 Reload - disables vsync by default and
     * runs into severe frame latency issues on Deck. */
    { R"(\\P3R\.exe$)", {{
      { "dxgi.syncInterval",                "1" },
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
          float&        result) {
    if (value.size() == 0)
      return false;

    // Parse sign
    size_t pos = 0;
    bool negate = false;

    if (value[0] == '-') {
      negate = true;

      if (++pos == value.size())
        return false;
    }

    // Parse integer part
    uint64_t intPart = 0;

    if (value[pos] == '.')
      return false;

    while (pos < value.size()) {
      if (value[pos] == '.') {
        if (++pos == value.size())
          return false;
        break;
      }

      if (value[pos] < '0' || value[pos] > '9')
        return false;

      intPart *= 10;
      intPart += value[pos] - '0';
      pos += 1;
    }

    // Parse fractional part
    uint64_t fractPart = 0;
    uint64_t fractDivisor = 1;

    while (pos < value.size()) {
      if (value[pos] < '0' || value[pos] > '9')
        return false;

      fractDivisor *= 10;
      fractPart *= 10;
      fractPart += value[pos] - '0';
      pos += 1;
    }

    // Compute final number, not super accurate but this should do
    result = float((double(fractPart) / double(fractDivisor)) + double(intPart));

    if (negate)
      result = -result;

    return std::isfinite(result);
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
    std::string confLine = env::getEnvVar("DXVK_CONFIG");

    if (filePath == "")
      filePath = "dxvk.conf";

    // Open the file if it exists
    std::ifstream stream(str::topath(filePath.c_str()).c_str());

    if (!stream && confLine.empty())
      return config;

    // Initialize parser context
    ConfigContext ctx;
    ctx.active = true;

    if (stream) {
      // Inform the user that we loaded a file, might
      // help when debugging configuration issues
      Logger::info(str::format("Found config file: ", filePath));

      // Parse the file line by line
      std::string line;

      while (std::getline(stream, line))
        parseUserConfigLine(config, ctx, line);
    }

    if (!confLine.empty()) {
      // Inform the user that we parsing config from environment, might
      // help when debugging configuration issues
      Logger::info(str::format("Found config env: ", confLine));

      for(auto l : str::split(confLine, ";"))
        parseUserConfigLine(config, ctx, std::string(l.data(), l.size()));
    }

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
