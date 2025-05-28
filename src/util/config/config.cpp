#include <array>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <utility>

#include "config.h"

#include "../log/log.h"

#include "../util_env.h"

#include "../sha1/sha1_util.h"

namespace dxvk {

  using ProfileList = std::vector<std::pair<const char*, Config>>;


  const static ProfileList g_profiles = {
    /**********************************************/
    /* D3D12 GAMES (vkd3d-proton with dxvk dxgi)  */
    /**********************************************/

    /* Diablo 4 - Will complain about missing  *
     * GPU unless dxgi Id match actual GPU Id. *
     * Bugs out on some multi-gpu systems.     */
    { R"(\\Diablo IV\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
      { "dxvk.hideIntegratedGraphics",      "True"  },
    }} },
    /* Ratchet & Clank: Rift Apart - does not allow
     * enabling ray tracing if it sees an AMD GPU. */
    { R"(\\RiftApart\.exe$)", {{
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Persona 3 Reload - disables vsync by default and
     * runs into severe frame latency issues on Deck. */
    { R"(\\P3R\.exe$)", {{
      { "dxgi.syncInterval",                "1" },
    }} },
    /* World of Warcraft                           *
     * Bugs out on some multi-gpu systems.         */
    { R"(\\Wow(Classic)?\.exe$)", {{
      { "dxvk.hideIntegratedGraphics",      "True"  },
    }} },
    /* Bright Memory - Will choose other vendors   *
     * over Intel even if Intel is the only dGPU   */
    { R"(\\BrightMemory_EP1-Win64-Shipping\.exe$)", {{
      { "dxvk.hideIntegratedGraphics",      "True"  },
    }} },
    /* AC Shadows: Uses composition swapchain to   *
     * check for HDR support                       */
    { R"(\\ACShadows\.exe$)", {{
      { "dxgi.enableDummyCompositionSwapchain", "True" }
    }} },

    /**********************************************/
    /* D3D11 GAMES                                */
    /**********************************************/

    /* Batman Arkham Knight - doesn't like intel vendor id 
      (refuses to boot if vendor isn't 0x10de or 0x1002)  */
    { R"(\\BatmanAK\.exe$)", {{
      { "dxgi.hideIntelGpu",                "True" },
    }} },
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
      { "d3d11.cachedDynamicResources",     "vi"   },
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
      { "d3d11.cachedDynamicResources",     "vi"   },
    }} },
    /* Hitman 2 - requires AGS library      */
    { R"(\\HITMAN2\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
      { "d3d11.cachedDynamicResources",     "c"    },
    }} },
    /* Modern Warfare Remastered                  */
    { R"(\\h1(_[ms]p64_ship|-mod)\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* H2M-Mod - Modern Warfare Remastered        */
    { R"(\\h2m-mod\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare 2 Campaign Remastered       *
     * AMD AGS crash same as above                */
    { R"(\\MW2CR\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Crysis 3 - slower if it notices AMD card     *
     * Cached dynamic buffers help massively        *
     * in CPU bound game parts                      */
    { R"(\\Crysis3\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
      { "d3d11.cachedDynamicResources",     "a"    },
    }} },
    /* Crysis 3 Remastered                          *
     * Cached dynamic buffers help massively        *
     * in CPU bound game parts                      */
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
      { "d3d11.relaxedGraphicsBarriers",    "True" },
    }} },
    /* God of War - relies on NVAPI/AMDAGS for    *
     * barrier stuff, needs nvapi for DLSS        */
    { R"(\\GoW\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
      { "dxgi.hideNvidiaGpu",               "False" },
      { "dxgi.maxFrameLatency",             "1"    },
    }} },
    /* AoE 2 DE - runs poorly for some users      */
    { R"(\\AoE2DE_s\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "a"    },
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
      { "d3d11.relaxedGraphicsBarriers",    "True" },
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
    /* Bladestorm Nightmare                       *
     * Game speed increases when above 60 fps in  *
     * the tavern area                            */
    { R"(\\BLADESTORM Nightmare\\Launch_(EA|JP)\.exe$)", {{
      { "dxgi.maxFrameRate",                "60"  },
    }} },
    /* Vindictus d3d11 CPU bound perf, and work   *
     * around the game not properly initializing  *
     * some of its constant buffers after discard */
    { R"(\\Vindictus(_x64)?\.exe$)", {{
      { "d3d11.cachedDynamicResources",     "cr"   },
      { "dxvk.zeroMappedMemory",            "True" },
    }} },
    /* Riders Republic - Statically linked AMDAGS */
    { R"(\\RidersRepublic(_BE)?\.exe$)", {{
      { "dxgi.hideAmdGpu",                "True"   },
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
      { "d3d11.relaxedGraphicsBarriers",    "True"  },
      { "d3d11.exposeDriverCommandLists",   "False" },
      { "dxgi.hideNvidiaGpu",               "False" },
    }} },
    /* Crysis 1/Warhead - Game bug in d3d10 makes *
     * it select lowest supported refresh rate    */
    { R"(\\Crysis(64)?\.exe$)", {{
      { "d3d9.maxFrameRate",              "-1"      },
      { "dxgi.maxFrameRate",              "-1"      },
    }} },
    /* EDF6 - possible race condition?            */
    { R"(\\EDF6\.exe$)", {{
      { "d3d11.enableContextLock",          "True" },
    }} },
    /* Kena: Bridge of Spirits: intel water       * 
     * flickering issues                          */
    { R"(\\Kena-Win64-Shipping\.exe$)", {{
      { "dxgi.hideIntelGpu",                 "True" },
    }} },
    /* GTA Definitive Edition trilogy             * 
     * Static ags crash with HDR support          */
    { R"(\\(LibertyCity|ViceCity|SanAndreas)\.exe$)", {{
      { "dxgi.enableUe4Workarounds",        "True" },
    }} },
    /* Warcraft 3 Reforged                         *
     * Bugs out on some multi-gpu systems.         */
    { R"(\\x86_64\\Warcraft III\.exe$)", {{
      { "dxvk.hideIntegratedGraphics",      "True"  },
    }} },
    /* Earth Defense Force 5                       */
    { R"(\\EDF5\.exe$)", {{
      { "dxgi.tearFree",                    "False" },
      { "dxgi.syncInterval",                "1"     },
    }} },
    /* The Hurricane of the Varstray               *
     * Too fast above 60fps                        */
    { R"(\\Varstray_steam(_demo)?\.exe$)", {{
      { "dxgi.maxFrameRate",                "60" },
    }} },
    /* Far Cry 5 and New Dawn                      *
     * Invisible terrain on Intel                  */
    { R"(\\FarCry(5|NewDawn)\.exe$)", {{
      { "d3d11.zeroInitWorkgroupMemory",    "True" },
    }} },
    /* Watch Dogs 2 - ships broken compute shaders *
     * with no barriers when they are needed       */
    { R"(\\WatchDogs2\.exe$)", {{
      { "d3d11.forceComputeUavBarriers",    "True" },
    }} },
    /* Rocketbirds 2: Ignores row pitch for mapped *
     * images, corrupting intro video              */
    { R"(\\Rocketbirds 2\\Game\.exe$)", {{
      { "d3d11.disableDirectImageMapping",  "True" },
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
    /* Borderlands                                */
    { R"(\\Borderlands\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
    }} },
    /* Borderlands 2                               *
     * Missing lava in Vault of the Warrior        *
     * without Strict floats                       */
    { R"(\\Borderlands2\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
      { "d3d9.supportDFFormats",            "False" },
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Borderlands: The Pre-Sequel                  */
    { R"(\\BorderlandsPreSequel\.exe$)", {{
      { "d3d9.lenientClear",                "True" },
      { "d3d9.supportDFFormats",            "False" },
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
    { R"(\\(Sims2.*|TS2BodyShop|SimsLS|SimsPS|SimsCS)"
      R"(|The Sims 2 Content Manager|TS2HomeCrafterPlus)\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "0091" },
      { "d3d9.customDeviceDesc",            "GeForce 7800 GTX" },
      { "d3d9.disableA8RT",                 "True" },
      { "d3d9.supportX4R4G4B4",             "False" },
      { "d3d9.maxAvailableMemory",          "2048" },
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.cachedDynamicBuffers",        "True" },
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
    /* Dead Space 2
       Physics issues above 60 FPS
       Built-in Vsync Locks the game to 30 FPS
    */
    { R"(\\deadspace2\.exe$)", {{
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
      { "d3d9.hideNvidiaGpu",               "True" },
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
    /* Skyrim (NVAPI)                             */
    { R"(\\TESV\.exe$)", {{
      { "d3d9.hideNvidiaGpu",               "True" },
    }} },
    /* Hyperdimension Neptunia U: Action Unleashed */
    { R"(\\Neptunia\.exe$)", {{
      { "d3d9.forceAspectRatio",            "16:9" },
    }} },
    /* GTA IV (NVAPI)                             */
    /* Also thinks we're always on Intel          *
     * and will report/use bad amounts of VRAM
     * if we report more than 128 MB of VRAM.
     * Disabling support for DF texture formats
     * makes the game use a better looking render
     * path for mirrors.
     * Also runs into issues after alt-tabbing.   */
    { R"(\\(GTAIV|EFLC)\.exe$)", {{
      { "d3d9.hideNvidiaGpu",               "True" },
      { "dxgi.maxDeviceMemory",             "128" },
      { "d3d9.supportDFFormats",            "False" },
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Battlefield 2 & Battlefield 2142           *
     * Bad z-pass and ingame GUI loss on alt tab  *
     * Also hang when alt tabbing which seems     *
     * like a game bug that d3d9 drivers work     *
     * around.                                    */
    { R"(\\(BF2|BF2142|PRBF2)\.exe$)", {{
      { "d3d9.deviceLossOnFocusLoss",       "True" },
      { "d3d9.countLosableResources",       "False"},
    }} },
    /* SpellForce 2 Series                        */
    { R"(\\SpellForce2.*\.exe$)", {{
      { "d3d9.forceSamplerTypeSpecConstants", "True" },
    }} },
    /* Tomb Raider: Legend, Anniversary, Underworld  *
     * Read from a buffer created with:              *
     * D3DPOOL_DEFAULT,                              *
     * D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY buffer. *
     * Legend flickers with next gen content option. */
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
    /* Warhammer: Online                         *
     * Overly bright ground textures on Nvidia   */
    { R"(\\(WAR(-64)?|WARTEST(-64)?)\.exe$)", {{
      { "d3d9.hideNvidiaGpu",               "True" },
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
    /* Time Leap Paradise SUPER LIVE             */
    { R"(\\tlpsl\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
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
      { "d3d9.hideAmdGpu",                  "True" },
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
       Work around partial presentation issues  */
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
    /* Majesty 2 (Collection)                   *
     * Crashes on UMA without a memory limit,   *
     * since the game(s) will allocate all      *
     * available VRAM on startup.               */
    { R"(\\Majesty2\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "2048" },
    }} },
    /* Myst V End of Ages                       *
     * Resolution change crash and cached       *
     * dynamic buffers for performance reasons  */
    { R"(\\eoa\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
      { "d3d9.countLosableResources",       "False" },
    }} },
    /* Supreme Commander & Forged Alliance Forever */
    { R"(\\(SupremeCommander|ForgedAlliance)\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Bionic Commando
       Physics break at high fps               */
    { R"(\\bionic_commando\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
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
    /* Sonic & All-Stars Racing Transformed    *
     * Helps performance when Resizable BAR    *
     * is enabled                              */
    { R"(\\ASN_App_PcDx9_Final\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
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
      { "d3d9.deferSurfaceCreation",        "True" },
      { "d3d9.cachedDynamicBuffers",        "True" },
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
    /* 9th Dawn II                               *
     * OpenGL game that also spins up d3d9       *
     * Black screens without config              */
    { R"(\\ninthdawnii\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* Delta Force: Xtreme 1 & 2                 *
     * Black screen on Alt-Tab and performance   */
    { R"(\\(DFX|dfx2)\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Prototype                                 *
     * Incorrect shadows on AMD & Intel.         *
     * AA 4x can not be selected above 2GB vram  */
    { R"(\\prototypef\.exe$)", {{
      { "d3d9.hideAmdGpu",                  "True" },
      { "dxgi.maxDeviceMemory",             "2047" },
    }} },
    /* STAR WARS: The Force Unleashed            *
     * Prevents black screen on each alt-tab     */
    { R"(\\SWTFU\.exe$)", {{ 
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Fallout New Vegas - Various visual issues *
     * with mods such as New Vegas Reloaded      */
    { R"(\\FalloutNV\.exe$)", {{ 
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Dungeons and Dragons: Dragonshard         *
     * Massive FPS decreases in some scenes      */
    { R"(\\Dragonshard\.exe$)", {{ 
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Battle for Middle-earth 2 and expansion   *
     * Slowdowns in certain scenarios            */
    { R"(\\(The Battle for Middle-earth( \(tm\))? II( Demo)?)"
      R"(|The Lord of the Rings, The Rise of the Witch-king)\\game\.dat$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* WRC4 - Audio breaks above 60fps           */
    { R"(\\WRC4\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* Splinter Cell Conviction - Alt-tab black  *
     * screen and unsupported GPU complaint      */
    { R"(\\conviction_game\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
      { "dxgi.customDeviceId",              "05e0" },
      { "dxgi.customDeviceDesc",            "GeForce GTX 295" },
    }} },
    /* Resident Evil: Operation Raccoon City     */
    { R"(\\RaccoonCity\.exe$)", {{
      { "d3d9.textureMemory",               "0" },
    }} },
    /* APB: Reloaded                               *
     * Fixes frametime jumps when shooting         */
    { R"(\\APB\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Battle Mages - helps CPU bound perf         */
    { R"(\\Battle Mages\\mages\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Prince of Persia (2008) - Can get stuck     *
     * during loading at very high fps             */
    { R"(\\Prince( of Persia|OfPersia_Launcher)\.exe$)", {{
      { "d3d9.maxFrameRate",                 "240" },
    }} },
    /* F.E.A.R 1 & expansions                      *
     * Graphics glitches at very high fps          */
    { R"(\\FEAR(MP|XP|XP2)?\.exe$)", {{
      { "d3d9.maxFrameRate",                 "360" },
    }} },
    /* Secret World Legends - d3d9 mode only sees  *
     * 512MB vram locking higher graphics presets  */
    { R"(\\SecretWorldLegends\.exe$)", {{
      { "d3d9.memoryTrackTest",              "True" },
    }} },
    /* Far Cry 2:                                  *
     * Set cached dynamic buffers to True to       *
     * improve perf on all hardware.               */
    { R"(\\(FarCry2|farcry2game)\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Dark Sector - Crashes in places             */
    { R"(\\DS\.exe$)", {{
      { "d3d9.textureMemory",                "0" },
    }} },
    /* Arcana Heart 3 Love Max + Xtend version     *
     * Game speed is too fast above 60 fps         */
    { R"(\\(AH3LM|AALib)\.exe$)", {{
      { "d3d9.maxFrameRate",                "60" },
    }} },
    /* May Payne 3 - Visual issues on some drivers *
     * such as ANV (and amdvlk when set to True)   */
    { R"(\\MaxPayne3\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Star Wars Empire at War & expansion         *
     * In base game the AA option dissapears at    *
     * 2075MB vram and above                       */
    { R"(\\(StarWarsG|sweaw|swfoc)\.exe$)", {{
      { "d3d9.maxAvailableMemory",          "2048" },
      { "d3d9.memoryTrackTest",             "True" },
    }} },
    /* CivCity: Rome                              *
     * Enables soft real-time shadows             */
    { R"(\\CivCity Rome\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
    }} },
    /* Silent Hill 2 (2001)                       *
     * The Enhancements mod configures the        *
     * swapchain to only have a single backbuffer *
     * and then calls GetFrontBufferData after    *
     * rendering to the backbuffer. This causes   *
     * it to get the wrong data from              *
     * GetFrontBufferData().                      */
    { R"(\\sh2pc\.exe$)", {{
      { "d3d9.extraFrontbuffer",            "True" },
    }} },
    /* Lego Indiana Jones: The Original Adventures *
     * Fix UI performance                          */
    { R"(\\LEGOIndy\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Lego Batman: The Videogame                 *
     * Fix UI performance                         */
    { R"((\\LEGOBatman|LegoBatman\\Game)\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Thumper - Fixes missing track              */
    { R"(\\THUMPER_dx9\.exe$)", {{
      { "d3d9.floatEmulation",              "Strict" },
    }} },
    /* Red Orchestra: Ostfront 41-45 - Blinks,    *
     * freeze, crash or goes Windowed on alt-tab  */
    { R"(\\RedOrchestra\.exe$)", {{
      { "d3d9.deviceLossOnFocusLoss",       "True" },
      { "d3d9.countLosableResources",       "False" },
    }} },
    /* Pirate Huner - Prevents crash              */
    { R"(\\PH\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "2048" },
    }} },
    /* Battle Engine Aquila - Enables additional  *
     * graphical features and Nvidia particle fog */
    { R"(\\BEA\.exe$)", {{
      { "d3d9.customVendorId",              "10de" },
      { "d3d9.customDeviceId",              "0330" },
      { "d3d9.customDeviceDesc",            "NVIDIA GeForce FX 5900 Ultra" },
    }} },

    /**********************************************/
    /* D3D8 GAMES                                 */
    /**********************************************/

    /* Duke Nukem Forever (2001)                  */
    { R"(\\DukeForever\.exe$)", {{
      { "d3d9.maxFrameRate",                "60"   },
    }} },
    /* Anito: Defend a Land Enraged              */
    { R"(\\Anito\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "1024" },
    }} },
    /* Red Faction                               *
     * Fixes crashing when starting a new game   */
    { R"(\\RF\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",   "False" },
    }} },
    /* Commandos 3                               *
     * The game doesn't use NOOVERWRITE properly *
     * and reads from actively modified buffers, *
     * which causes graphical glitches at times  */
    { R"(\\Commandos3\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",   "False" },
    }} },
    /* Motor City Online                         */
    { R"(\\MCity_d\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
      { "d3d8.batching",                    "True" },
    }} },
    /* Railroad Tycoon 3                         */
    { R"(\\RT3\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
    }} },
    /* Pure Pinball 2.0 REDUX                    *
     * This game reads from undeclared vs inputs *
     * but somehow works on native. Let's just   *
     * change its declaration to make them work. */
    { R"(\\Pure Pinball 2\.0 REDUX\.exe$)", {{
      { "d3d8.forceVsDecl",  "0:2,4:2,7:4,9:1,8:1" },
    }} },
    /* Need for Speed III: Hot Pursuit           *
       (with the "Modern Patch")                 */
    { R"(\\nfs3\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
      { "d3d8.batching",                    "True" },
    }} },
    /* Need for Speed: High Stakes / Road         *
       Challenge (with the "Modern Patch") -      *
       Won't actually render anything in game     *
       without a memory limit in place            */
    { R"(\\nfs4\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",           "256" },
      { "d3d8.batching",                    "True" },
    }} },
    /* Need for Speed: Hot Pursuit 2              */
    { R"(\\NFSHP2\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Project I.G.I. 2: Covert Strike            */
    { R"(\\igi2\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Treasure Planet: Battle at Procyon        *
     * Declares v5 as color but shader uses v6   */
    { R"(\\TP_Win32\.exe$)", {{
      { "d3d8.forceVsDecl",      "0:2,3:2,6:4,7:1" },
    }} },
    /* Scrapland (Remastered)                   */
    { R"(\\Scrap\.exe$)", {{
      { "d3d9.deferSurfaceCreation",        "True" },
    }} },
    /* V-Rally 3                                  */
    { R"(\\VRally3(Demo)?\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
    }} },
    /* Soldiers: Heroes Of World War II           *
     * Fills up all available memory and hangs    *
     * while loading the main menu otherwise      */
    { R"(\\Soldiers\.exe$)", {{
      { "d3d9.memoryTrackTest",             "True" },
      { "d3d9.maxAvailableMemory",          "512"  },
    }} },
    /* Cossacks II: Napoleonic Wars &             *
     * Battle for Europe                          */
    { R"(\\Cossacks II.*\\engine\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
    }} },
    /* Alexander                                  */
    { R"(\\Alexander\\Data\\engine\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
    }} },
    /* 3DMark2001 (SE)                            *
     * Fixes a drastic performance drop in the    *
     * "Car Chase - High Detail" benchmark        */
    { R"(\\3DMark2001(SE)?\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",   "False" },
    }} },
    /* Delta Force: Black Hawk Down               */
    { R"(\\dfbhd\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* X2: The Threat                             */
    { R"(\\X2\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* The Lord of the Rings:                     *
     * The Fellowship of the Ring                 */
    { R"(\\Fellowship\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
      { "d3d8.placeP8InScratch",            "True" },
    }} },
    /* Inquisitor (2009)                          *
     * Leaks a resource when alt-tabbing          */
    { R"(\\Inquisitor\.exe$)", {{
      { "d3d9.countLosableResources",      "False" },
    }} },
    /* Art of Murder FBI Confidential - CPU perf  */
    { R"(\\Art of Murder - FBI Confidential\\game\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Max Payne 1 - Stalls waiting for an index buffer */
    { R"(\\MaxPayne\.exe$)", {{
      { "d3d9.allowDirectBufferMapping",   "False" },
    }} },
    /* Z: Steel Soldiers                          */
    { R"(\\z2\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* FIFA Football 2003                         */
    { R"(\\fifa2003(demo)?\.exe$)", {{
      { "d3d9.cachedDynamicBuffers",        "True" },
    }} },
    /* Splinter Cell: Pandora Tomorrow            *
     * Broken inputs and physics above 60 FPS     */
    { R"(\\SplinterCell2\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
      { "d3d8.scaleDref",                     "24" },
    }} },
    /* Chrome: Gold Edition                       *
     * Broken character model motion at high FPS  */
    { R"(\\Chrome(Single|Net)\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
    }} },
    /* Rayman 3: Hoodlum Havoc                    *
     * Missing geometry and textures without      *
     * legacy DISCARD behavior                    */
    { R"(\\Rayman3\.exe$)", {{
      { "d3d9.maxFrameRate",                  "60" },
      { "d3d8.forceLegacyDiscard",          "True" },
    }} },
    /* Tom Clancy's Splinter Cell                 *
     * Fixes shadow buffers and alt-tab           */
    { R"(\\splintercell\.exe$)", {{
      { "d3d8.scaleDref",                     "24" },
      { "d3d8.shadowPerspectiveDivide",     "True" },
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
    /* Trainz v1.3 (2001)                         *
     * Fixes black screen after alt-tab           */
    { R"(\\bin\\trainz\.exe$)", {{
      { "d3d9.deviceLossOnFocusLoss",       "True" },
    }} },
  };


  const static ProfileList g_deckProfiles = {
    /* Fallout 4: Defaults to 45 FPS on OLED, but also breaks above 60 FPS */
    { R"(\\Fallout4\.exe$)", {{
      { "dxgi.syncInterval",                "1" },
      { "dxgi.maxFrameRate",                "60" },
    }} },
  };


  const static ProfileList g_hashedProfiles = {
    /* Nothing to see here */
  };


  const Config* findProfile(const ProfileList& profiles, const std::string& appName) {
    auto appConfig = std::find_if(profiles.begin(), profiles.end(),
      [&appName] (const std::pair<const char*, Config>& pair) {
        // With certain locales, regex parsing will simply crash. Using regex::imbue
        // does not resolve this; only the global locale seems to matter here. Catch
        // bad_alloc errors to work around this for now.
        try {
          std::regex expr(pair.first, std::regex::extended | std::regex::icase);
          return std::regex_search(appName, expr);
        } catch (const std::bad_alloc& e) {
          Logger::err(str::format("Failed to parse regular expression: ", pair.first));
          return false;
        }
      });

    return appConfig != profiles.end()
      ? &appConfig->second
      : nullptr;
  }


  const Config* findHashedProfile(const ProfileList& profiles, const std::string& appName) {
    // Don't bother hashing exe names if we don't have
    // any top-secret app profiles to begin with
    if (profiles.empty())
      return nullptr;

    auto n = appName.find_last_of('\\') + 1u;

    if (n >= appName.size())
      return nullptr;

    auto hash = Sha1Hash::compute(&appName[n], appName.size() - n).toString();

    auto appConfig = std::find_if(profiles.begin(), profiles.end(),
      [&hash] (const std::pair<const char*, Config>& pair) {
        return hash == pair.first;
      });

    return appConfig != profiles.end()
      ? &appConfig->second
      : nullptr;
  }


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
    const Config* config = nullptr;

    if (env::getEnvVar("SteamDeck") == "1")
      config = findProfile(g_deckProfiles, appName);

    if (!config)
      config = findProfile(g_profiles, appName);

    if (!config)
      config = findHashedProfile(g_hashedProfiles, appName);

    if (config) {
      // Inform the user that we loaded a default config
      Logger::info(str::format("Found built-in config:"));

      for (auto& pair : config->m_options)
        Logger::info(str::format("  ", pair.first, " = ", pair.second));

      return *config;
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
      ctx.active = true;

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
