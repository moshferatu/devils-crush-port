// SPDX-License-Identifier: GPL-3.0-or-later
#include "proven_replace_hooks.hpp"

namespace devils_crush::app::host {

namespace {

// Deployable at any call.
constexpr RoutineKey kCachedPositionRestore{0x00U, 0xF241U};

// Deployable when every call is replaced (`@all`).
constexpr RoutineKey kProvenRoutines[] = {
    {0x00U, 0xF1B4U}, // position integration
    {0x01U, 0xDD09U}, // motion damping
    {0x01U, 0xD1A2U}, // motion rotation
    {0x01U, 0xD09EU}, // flipper projection
    {0x01U, 0xD612U}, // flipper radius projection
    {0x01U, 0xD62EU}, // flipper angle projection
    {0x01U, 0xCFD8U}, // collision motion adjustment
    {0x03U, 0x7FB1U}, // collision response
    {0x03U, 0x7F57U}, // collision consumer
    {0x03U, 0x7F1AU}, // initial collision consumer
    {0x07U, 0x4003U}, // bank7 mode gate
    {0x00U, 0xF55CU}, // score accumulator
    {0x00U, 0xF54FU}, // score repeat wrapper
    {0x00U, 0xE8B3U}, // rng update
    {0x00U, 0xE837U}, // signed word negation
    {0x00U, 0xE85DU}, // unsigned square root
    {0x00U, 0xE905U}, // signed division
    {0x00U, 0xE728U}, // unsigned fixed division
    {0x00U, 0xE7D7U}, // unsigned 24 division
    {0x00U, 0xE6DCU}, // unsigned byte division
    {0x00U, 0xE624U}, // unsigned multiply round
    {0x00U, 0xE569U}, // unsigned byte multiply
    {0x00U, 0xE5BCU}, // unsigned word multiply partial
    {0x00U, 0xE5ECU}, // signed word multiply
    {0x02U, 0x4FC4U}, // side gate deadline check
    {0x00U, 0xE65CU}, // signed fixed multiply
    {0x00U, 0xF4FCU}, // actor camera activation
    {0x01U, 0xC574U}, // actor slot reset
    {0x01U, 0xD466U}, // flipper input gate
    {0x01U, 0xD4B3U}, // flipper release arm
    // flipper active update
    {0x01U, 0xD458U}, {0x01U, 0xD48EU}, {0x01U, 0xD491U}, {0x01U, 0xD494U},
    {0x01U, 0xD4C6U}, {0x01U, 0xD4C9U}, {0x01U, 0xD4CCU},
    {0x01U, 0xCC1AU}, // collision scan vertical reject
    {0x01U, 0xCC98U}, // collision mask lookup
    {0x01U, 0xCCD5U}, // collision geometry
    {0x01U, 0xD500U}, // flipper phase step
    // flipper collision response continuation
    {0x01U, 0xD525U}, {0x01U, 0xD52CU}, {0x01U, 0xD53BU}, {0x01U, 0xD54AU},
    {0x01U, 0xD54DU}, {0x01U, 0xD550U}, {0x01U, 0xD553U}, {0x01U, 0xD556U},
    {0x01U, 0xD589U}, {0x01U, 0xD594U}, {0x01U, 0xD599U}, {0x01U, 0xD5A9U},
    {0x01U, 0xD5B8U}, {0x01U, 0xD5BBU}, {0x01U, 0xD5BEU}, {0x01U, 0xD5C1U},
    {0x01U, 0xD5C4U}, {0x01U, 0xD5D8U},
    {0x01U, 0xD081U}, // flipper geometry
    {0x01U, 0xC5C9U}, // coroutine scheduler
    {0x01U, 0xC3CBU}, // nudge input gate
    {0x01U, 0xCBF9U}, // collision vector rts
    {0x01U, 0xC49EU}, // session input gate
    {0x01U, 0xDD6EU}, // ball drain threshold
    {0x01U, 0xDC95U}, // motion substep split
    // ball substep loop
    {0x01U, 0xC6C2U}, {0x01U, 0xC6C5U}, {0x01U, 0xC6D9U}, {0x01U, 0xC6E6U},
    {0x01U, 0xC6E9U}, {0x01U, 0xC6EBU}, {0x01U, 0xC6F2U}, {0x01U, 0xC6F5U},
    {0x01U, 0xC708U}, {0x01U, 0xC721U}, {0x01U, 0xC72EU}, {0x01U, 0xC742U},
    {0x01U, 0xC74CU}, {0x01U, 0xC759U}, {0x01U, 0xC75CU}, {0x01U, 0xC75EU},
    {0x01U, 0xC761U}, {0x01U, 0xC768U}, {0x01U, 0xC76BU}, {0x01U, 0xC76EU},
    // actor motion step
    {0x00U, 0xF08EU}, {0x00U, 0xF09AU}, {0x00U, 0xF0A4U}, {0x00U, 0xF0AEU},
    {0x00U, 0xF0B8U}, {0x00U, 0xF0C0U}, {0x00U, 0xF0FEU}, {0x00U, 0xF104U},
    {0x00U, 0xF142U}, {0x00U, 0xF148U}, {0x00U, 0xF17EU},
    {0x01U, 0xC80AU}, // remaining ball presentation
    {0x01U, 0xC7EAU}, // remaining ball transition gate
    {0x01U, 0xC7E5U}, // remaining ball decision
    {0x02U, 0x4DD9U}, // giant skeleton eyeball gate
    {0x02U, 0x4DC8U}, // giant skeleton eyeball initializer
    {0x02U, 0x4DF7U}, // giant skeleton eyeball opening wait
    {0x02U, 0x4E24U}, // giant skeleton eyeball steady idle
    // giant skeleton eyeball final wait
    {0x02U, 0x4F3DU}, {0x02U, 0x4F6CU},
    {0x03U, 0x6876U}, // gray blob initializer
    {0x03U, 0x68A1U}, // gray blob first live prefix
    {0x02U, 0x57ADU}, // sorcerer live controller gate
    {0x02U, 0x4F9FU}, // mercy star initializer
    {0x02U, 0x50EEU}, // devil shooter initializer
    {0x02U, 0x44E4U}, // little skull initializer
    {0x02U, 0x458AU}, // little skull gray initializer
    {0x02U, 0x45A4U}, // little skull gray release
    {0x02U, 0x4515U}, // little skull live continuation
    {0x02U, 0x4485U}, // little skull pair initializer
    {0x02U, 0x4455U}, // little skull hidden initializer
    {0x02U, 0x565CU}, // big star initializer
    {0x01U, 0xC7F4U}, // zero ball scheduler
    {0x01U, 0xC7EFU}, // alternate ball scheduler
    {0x01U, 0xC7F9U}, // zero ball countdown tick
    {0x01U, 0xC7FCU}, // zero ball expiry scheduler
    {0x01U, 0xC804U}, // zero ball presentation gate
    {0x01U, 0xC812U}, // zero ball state gate
    {0x02U, 0x5808U}, // sorcerer deployment initializer
    {0x02U, 0x57F8U}, // sorcerer staging initializer
    {0x02U, 0x433FU}, // dragon egg initializer
    {0x03U, 0x61A0U}, // undead head initializer
    {0x03U, 0x61ABU}, // undead head selector gate
    {0x03U, 0x61A3U}, // undead head waiting parent
    // side worm initializers
    {0x02U, 0x5C4BU}, {0x02U, 0x5C7CU},
    {0x02U, 0x5CCDU}, // side worm selector gate
    {0x02U, 0x5CC5U}, // side worm waiting parent
    {0x02U, 0x5D4AU}, // side worm active timer initializer
    {0x02U, 0x5A31U}, // dragon head initializer
    {0x02U, 0x5EE4U}, // giant skeleton initializer
    {0x02U, 0x5EECU}, // giant skeleton controller
    {0x03U, 0x664AU}, // three blobs initializer
    {0x03U, 0x65BCU}, // three blobs controller initializer
    {0x03U, 0x6319U}, // walled star initializer
    {0x02U, 0x4A44U}, // walled star access initializer
    // side star initializers
    {0x02U, 0x4A54U}, {0x02U, 0x4A4CU},
    // knight initializers
    {0x03U, 0x734EU}, {0x03U, 0x7353U}, {0x03U, 0x735AU}, {0x03U, 0x7361U},
    {0x03U, 0x64FAU}, // side spider initializer
    {0x03U, 0x646BU}, // front spider initializer
    {0x02U, 0x4C78U}, // giant face helmet initializer
    // giant face helmet outer initializers
    {0x02U, 0x4A5CU}, {0x02U, 0x4A64U},
    {0x02U, 0x4FC1U}, // mercy star gate
    {0x02U, 0x42F6U}, // dragon egg controller
    {0x02U, 0x4345U}, // dragon egg hit gate
    {0x02U, 0x4357U}, // dragon egg hit continuation
    {0x02U, 0x4371U}, // dragon egg post allocation
    {0x02U, 0x4387U}, // dragon egg reset gate
    {0x02U, 0x445BU}, // big star feature gate
    {0x02U, 0x570AU}, // big star pending gate
    {0x02U, 0x5722U}, // big star pending helper
    {0x02U, 0x5738U}, // big star pending right helper
    {0x02U, 0x5674U}, // big star rotation wait
    // devil shooter companion dormant
    {0x02U, 0x53DDU}, {0x02U, 0x5456U},
    {0x02U, 0x5397U}, // devil shooter companion position
    {0x02U, 0x5408U}, // devil shooter companion appearance
    {0x02U, 0x545DU}, // devil shooter companion active timer
    {0x02U, 0x53E4U}, // devil shooter companion left active timer
    {0x02U, 0x53C8U}, // devil shooter left active controller
    {0x02U, 0x5441U}, // devil shooter right active controller
    {0x02U, 0x5A4BU}, // dragon head hit gate
    {0x02U, 0x5A6EU}, // dragon head hit continuation
    {0x02U, 0x5116U}, // devil shooter hit gate
    {0x02U, 0x5159U}, // devil shooter release delay gate
    {0x02U, 0x518DU}, // devil shooter charge after audio
    {0x02U, 0x5197U}, // devil shooter charge tail
    {0x02U, 0x5475U}, // devil shooter appearance submission prefix
    {0x02U, 0x5481U}, // devil shooter appearance submission tail
    {0x02U, 0x525FU}, // devil shooter post launch gate
    {0x02U, 0x520BU}, // devil shooter launch continuation
    // devil shooter charge retraction calls
    {0x02U, 0x5208U}, {0x02U, 0x52C3U},
    {0x02U, 0x5223U}, // devil shooter charge entry controller
    // upper parent animation
    {0x09U, 0x705CU}, {0x09U, 0x705FU}, {0x09U, 0x7084U}, {0x09U, 0x7087U},
    {0x09U, 0x7094U}, {0x09U, 0x709EU}, {0x09U, 0x70A7U}, {0x09U, 0x7194U},
    {0x09U, 0x7197U}, {0x09U, 0x71BCU}, {0x09U, 0x71BFU}, {0x09U, 0x71CDU},
    // scene layout record loop
    {0x02U, 0x40A8U}, {0x02U, 0x40BDU}, {0x02U, 0x4090U}, {0x02U, 0x4099U},
    // scene layout record loader
    {0x02U, 0x40F6U}, {0x02U, 0x40FEU}, {0x02U, 0x4096U}, {0x02U, 0x40FBU},
    {0x02U, 0x40A7U},
    {0x02U, 0x5249U}, // devil shooter charge controller
    {0x02U, 0x524CU}, // devil shooter charge controller tail
    {0x02U, 0x52E3U}, // devil shooter charge position
    {0x02U, 0x533AU}, // devil shooter charge return
    {0x02U, 0x52B8U}, // devil shooter catch continuation
    {0x02U, 0x52C6U}, // devil shooter catch retraction tail
    {0x02U, 0x54B9U}, // skeleton insect left gate
    {0x02U, 0x54D0U}, // skeleton insect startup countdown
    {0x02U, 0x5500U}, // skeleton insect left post trigger
    {0x02U, 0x5551U}, // skeleton insect right gate
    {0x02U, 0x5568U}, // skeleton insect right startup countdown
    {0x02U, 0x5590U}, // skeleton insect right post trigger
    {0x02U, 0x55F9U}, // skeleton insect center gate
    {0x02U, 0x5604U}, // skeleton insect center continuation
    {0x02U, 0x561AU}, // skeleton insect center toggle
    {0x02U, 0x5625U}, // skeleton insect center post delay handoff
    {0x02U, 0x5630U}, // skeleton insect center opening gate
    {0x03U, 0x63F1U}, // devils coffin gate
    {0x03U, 0x63E1U}, // devils coffin initializer
    {0x03U, 0x640AU}, // devils coffin opening
    {0x03U, 0x6445U}, // devils coffin closing
    {0x03U, 0x65C2U}, // three blobs controller
    {0x03U, 0x67B3U}, // three blobs appearance gate
    {0x03U, 0x62B8U}, // giant door hit gate
    {0x03U, 0x62CBU}, // giant door hit continuation
    {0x03U, 0x62D4U}, // giant door completion score prefix
    {0x03U, 0x62E2U}, // giant door completion scheduler prefix
    {0x03U, 0x62F0U}, // giant door completion request prefix
    {0x03U, 0x62FEU}, // giant door completion wait gate
    {0x03U, 0x6308U}, // giant door completion channel advance
    {0x03U, 0x6294U}, // giant door initializer
    {0x03U, 0x767EU}, // giant face animator gate
    {0x03U, 0x76C5U}, // giant face animator active wait
    {0x03U, 0x74D8U}, // giant face request gate
    {0x03U, 0x76FAU}, // giant face crown gate
    {0x03U, 0x7726U}, // giant face mouth gate
    {0x02U, 0x4C91U}, // giant face helmet controller
    {0x02U, 0x4CF1U}, // giant face helmet shared return
    {0x02U, 0x4CFBU}, // giant face helmet hit continuation
    {0x02U, 0x4D0BU}, // giant face helmet followup timer
    {0x02U, 0x4A93U}, // four star wall pocket controller
    {0x02U, 0x4B3FU}, // walled star access controller
    // side star access controller
    {0x02U, 0x4BD5U}, {0x02U, 0x4C0AU},
    {0x02U, 0x4AD2U}, // four star wall pocket post audio score
    {0x02U, 0x4AE5U}, // four star wall pocket post appearance
    {0x02U, 0x4AEBU}, // four star wall pocket animation
    {0x02U, 0x4C33U}, // side star bonus entrance animation
    {0x02U, 0x5972U}, // upper wall pocket hand initializer
    {0x02U, 0x5991U}, // upper wall pocket hand countdown
    {0x02U, 0x59A5U}, // upper wall pocket hand opening
    {0x02U, 0x59C7U}, // upper wall pocket hand opening followup
    {0x02U, 0x59E0U}, // upper wall pocket hand opening expiry
    {0x02U, 0x5A08U}, // upper wall pocket hand post opening schedule
    {0x02U, 0x5A0BU}, // upper wall pocket hand post opening state
    {0x02U, 0x5A1BU}, // upper wall pocket hand post opening countdown
    {0x02U, 0x5975U}, // upper wall pocket hand live setup
    {0x02U, 0x5986U}, // upper wall pocket hand live suffix
    {0x03U, 0x6339U}, // walled star hit gate
    {0x03U, 0x638FU}, // walled star post destruction idle
    {0x03U, 0x60C3U}, // tower hit gate
    {0x03U, 0x60F9U}, // tower bonus terminal
    {0x03U, 0x6136U}, // tower bonus continuation
    {0x03U, 0x60B3U}, // tower initializer
    {0x03U, 0x73D8U}, // knight hit gate
    {0x03U, 0x73FEU}, // knight terminal score prefix
    {0x03U, 0x7418U}, // knight terminal release prefix
    {0x03U, 0x740AU}, // knight terminal acknowledgement prefix
    {0x03U, 0x7420U}, // knight terminal scheduler
    {0x02U, 0x41A0U}, // knight terminal release helper prefix
    {0x02U, 0x41A8U}, // knight terminal release helper setup prefix
    {0x02U, 0x41ADU}, // knight terminal release helper table setup prefix
    {0x02U, 0x41C2U}, // knight terminal release helper call prefix
    {0x02U, 0x41F5U}, // knight terminal release helper pointer prefix
    {0x02U, 0x420BU}, // knight terminal release helper shared call prefix
    {0x02U, 0x420EU}, // knight terminal release helper post return mode branch prefix
    {0x02U, 0x4210U}, // knight terminal release helper post return pointer low load prefix
    {0x02U, 0x4212U}, // knight terminal release helper post return pointer low store prefix
    {0x02U, 0x4215U}, // knight terminal release helper post return pointer high load prefix
    {0x02U, 0x4217U}, // knight terminal release helper post return pointer high store prefix
    {0x02U, 0x421AU}, // knight terminal release helper post return scratch clear prefix
    {0x02U, 0x421CU}, // knight terminal release helper post return mode load prefix
    {0x02U, 0x421EU}, // knight terminal release helper post return mode signed branch prefix
    {0x02U, 0x4222U}, // knight terminal release helper post return position carry clear prefix
    {0x02U, 0x4223U}, // knight terminal release helper post return position base load prefix
    {0x02U, 0x4226U}, // knight terminal release helper post return position mode add prefix
    {0x02U, 0x4228U}, // knight terminal release helper post return position store prefix
    {0x02U, 0x422BU}, // knight terminal release helper post return position high load prefix
    {0x02U, 0x422EU}, // knight terminal release helper post return position high add prefix
    {0x02U, 0x4230U}, // knight terminal release helper post return position high store prefix
    {0x02U, 0x4233U}, // knight terminal release helper post return position high scratch clear prefix
    {0x02U, 0x4235U}, // knight terminal release helper post return position high mode load prefix
    {0x02U, 0x4237U}, // knight terminal release helper post return position high mode signed branch prefix
    {0x02U, 0x423BU}, // knight terminal release helper post return position high carry clear prefix
    {0x02U, 0x423CU}, // knight terminal release helper post return position high base load prefix
    {0x02U, 0x423FU}, // knight terminal release helper post return position high scratch add prefix
    {0x02U, 0x4241U}, // knight terminal release helper post return position high actor store prefix
    {0x02U, 0x4244U}, // knight terminal release helper post return actor field load prefix
    {0x02U, 0x4247U}, // knight terminal release helper post return actor field scratch add prefix
    {0x02U, 0x4249U}, // knight terminal release helper post return actor field store prefix
    {0x02U, 0x424CU}, // knight terminal release helper post return actor field clear prefix
    {0x02U, 0x424DU}, // knight terminal release helper post return actor field zero store prefix
    {0x02U, 0x4250U}, // knight terminal release helper post return actor field second zero store prefix
    {0x02U, 0x4253U}, // knight terminal release helper post return actor attribute load prefix
    {0x02U, 0x4256U}, // knight terminal release helper post return actor attribute mask prefix
    {0x02U, 0x4258U}, // knight terminal release helper post return actor attribute mode merge prefix
    {0x02U, 0x425AU}, // knight terminal release helper post return actor attribute store prefix
    {0x02U, 0x425DU}, // knight terminal release helper post return actor field constant load prefix
    {0x02U, 0x425FU}, // knight terminal release helper post return actor field constant store prefix
    {0x02U, 0x4262U}, // knight terminal release helper post return actor field second constant load prefix
    {0x02U, 0x4264U}, // knight terminal release helper post return actor field second constant store prefix
    {0x01U, 0xC024U}, // knight terminal release helper shared jump prefix
    {0x01U, 0xC5ABU}, // knight terminal release helper shared scan initializer prefix
    {0x01U, 0xC5ADU}, // knight terminal release helper shared scan read prefix
    {0x01U, 0xC5B0U}, // knight terminal release helper shared scan nonzero branch prefix
    {0x01U, 0xC5B2U}, // knight terminal release helper shared scan increment prefix
    {0x01U, 0xC5B3U}, // knight terminal release helper shared scan range compare prefix
    {0x01U, 0xC5B5U}, // knight terminal release helper shared scan loop branch prefix
    {0x01U, 0xC5B9U}, // knight terminal release helper allocator load prefix
    {0x01U, 0xC5BBU}, // knight terminal release helper allocator store prefix
    {0x01U, 0xC5BEU}, // knight terminal release helper allocator mode load prefix
    {0x01U, 0xC5C0U}, // knight terminal release helper allocator mode branch prefix
    {0x01U, 0xC5C7U}, // knight terminal release helper allocator return carry clear prefix
    {0x03U, 0x7378U}, // knight waiting parent
    {0x03U, 0x73A6U}, // knight movement parent
    {0x03U, 0x65A4U}, // spider appearance
    // spider controller parents
    {0x03U, 0x6490U}, {0x03U, 0x651FU},
    // front spider scheduler call seams
    {0x03U, 0x64D2U}, {0x03U, 0x64DDU}, {0x03U, 0x64EDU},
    {0x02U, 0x533BU}, // devil shooter charge retraction
    {0x03U, 0x6054U}, // tower insect animation
    {0x02U, 0x42B0U}, // impact burst lifetime
    {0x02U, 0x42A3U}, // impact burst audio command call
    {0x02U, 0x427CU}, // impact burst allocator success return
    // tower insect initial controller
    {0x02U, 0x5FDBU}, {0x02U, 0x5FE6U},
    // tower insect controller
    {0x03U, 0x6016U}, {0x02U, 0x5FFBU}, {0x03U, 0x601CU}, {0x03U, 0x601EU},
    // small soldier source gates
    {0x03U, 0x7D85U}, {0x03U, 0x7E23U},
    // small soldier source initializers
    {0x03U, 0x7D4DU}, {0x03U, 0x7DE9U},
    // small soldier delayed spawn gates
    {0x03U, 0x7D6FU}, {0x03U, 0x7E0DU},
    // small soldier child allocation seams
    {0x03U, 0x7E87U}, {0x03U, 0x7E8AU}, {0x03U, 0x7EEEU},
    // collision effect allocator
    {0x03U, 0x7BF6U}, {0x03U, 0x7C02U}, {0x03U, 0x7C05U},
    // small soldier initial controller
    {0x03U, 0x7EFAU}, {0x03U, 0x7F05U},
    // giant face controller initializers
    {0x03U, 0x7428U}, {0x03U, 0x742BU}, {0x03U, 0x742EU},
    {0x03U, 0x766CU}, // giant face animator initializer
    {0x03U, 0x76F7U}, // giant face crown initializer
    {0x03U, 0x7718U}, // giant face mouth initializer
    {0x02U, 0x5F5FU}, // tower insect source gate
    {0x02U, 0x5F4AU}, // tower insect source countdown wait
    {0x02U, 0x5F5BU}, // tower insect source countdown return
    // tower insect child spawner seams
    {0x02U, 0x5F78U}, {0x02U, 0x5F7BU}, {0x02U, 0x5F7EU}, {0x02U, 0x5FD6U},
    {0x02U, 0x5F22U}, // tower insect source initializer
    {0x02U, 0x57E9U}, // sorcerer color gate
    {0x03U, 0x60E0U}, // tower hit continuation
    {0x03U, 0x611CU}, // tower post dispatch
    {0x03U, 0x60A6U}, // tower guard selector
    {0x03U, 0x78C7U}, // bumper hit gate
    // bumper initializers
    {0x03U, 0x789AU}, {0x03U, 0x78A4U},
    {0x03U, 0x79DEU}, // bumper child initializer
    {0x03U, 0x78F5U}, // bumper primary progress
    {0x03U, 0x7912U}, // bumper secondary progress
    {0x03U, 0x7CAFU}, // fixed response gate
    {0x03U, 0x7B27U}, // fixed scheduler gate
    {0x03U, 0x7B1FU}, // fixed scheduler initializer
    {0x03U, 0x7B6DU}, // fixed marker initializer
    {0x03U, 0x7CA7U}, // fixed response initializer
    {0x02U, 0x42EBU}, // dragon egg controller initializer
    // ranked control seam
    {0x09U, 0x62E0U}, {0x09U, 0x62E6U}, {0x09U, 0x62E8U}, {0x09U, 0x62E2U},
    {0x09U, 0x60BAU}, {0x09U, 0x62EEU}, {0x09U, 0x60BFU}, {0x09U, 0x608DU},
    {0x09U, 0x6090U}, {0x09U, 0x6095U}, {0x09U, 0x6099U}, {0x02U, 0x4D30U},
    {0x03U, 0x7AE0U}, {0x03U, 0x7AEBU}, {0x03U, 0x7AEFU}, {0x45U, 0x4148U},
    {0x01U, 0xD7B3U}, {0x03U, 0x66A8U}, {0x03U, 0x66B0U}, {0x03U, 0x7ACAU},
    {0x03U, 0x7AD5U}, {0x02U, 0x5A05U}, {0x03U, 0x61A9U}, {0x01U, 0xC9BBU},
    {0x02U, 0x5A00U}, {0x02U, 0x4D32U}, {0x02U, 0x56A5U}, {0x02U, 0x56A9U},
    {0x02U, 0x5682U}, {0x01U, 0xD902U}, {0x01U, 0xD915U}, {0x01U, 0xD92FU},
    {0x01U, 0xD907U}, {0x01U, 0xC9B3U}, {0x01U, 0xC9CBU}, {0x01U, 0xC9D1U},
    {0x01U, 0xC9AFU}, {0x01U, 0xC9C7U}, {0x01U, 0xC9C9U}, {0x01U, 0xC9CDU},
    {0x01U, 0xC9B1U}, {0x01U, 0xC9B5U}, {0x01U, 0xC9B9U}, {0x01U, 0xC9B7U},
    {0x01U, 0xC9BFU}, {0x00U, 0xEB1AU}, {0x00U, 0xE32CU}, {0x01U, 0xD62DU},
    {0x01U, 0xD68DU}, {0x00U, 0xE32FU}, {0x00U, 0xF55BU}, {0x03U, 0x7D6FU},
    {0x45U, 0x4B36U}, {0x03U, 0x657CU}, {0x09U, 0x7040U}, {0x09U, 0x7178U},
    {0x09U, 0x7022U}, {0x09U, 0x715AU}, {0x02U, 0x5004U}, {0x02U, 0x5007U},
    {0x02U, 0x5009U}, {0x02U, 0x500CU}, {0x02U, 0x500EU}, {0x02U, 0x5013U},
    {0x03U, 0x7AE2U}, {0x03U, 0x7AE5U}, {0x03U, 0x7AE8U}, {0x03U, 0x7AEDU},
    {0x03U, 0x7957U}, {0x02U, 0x4D40U}, {0x03U, 0x788EU}, {0x03U, 0x76C8U},
    {0x03U, 0x76D3U}, {0x03U, 0x76CDU}, {0x03U, 0x76D6U}, {0x03U, 0x76ECU},
    {0x01U, 0xC8DDU}, {0x03U, 0x7DCCU}, {0x01U, 0xC8DFU}, {0x03U, 0x7DD5U},
    {0x03U, 0x7DD8U}, {0x00U, 0xE5BAU}, {0x00U, 0xE59FU}, {0x02U, 0x4D34U},
    {0x02U, 0x5677U}, {0x03U, 0x76EFU}, {0x02U, 0x4D3BU}, {0x03U, 0x76D0U},
    {0x02U, 0x567AU}, {0x01U, 0xC9C1U}, {0x01U, 0xC9BDU}, {0x01U, 0xC9CFU},
    {0x03U, 0x76E9U}, {0x02U, 0x4D53U}, {0x02U, 0x4D2DU}, {0x01U, 0xC8DAU},
    {0x01U, 0xDA09U}, {0x03U, 0x61A6U}, {0x00U, 0xE5A4U}, {0x00U, 0xE5A6U},
    {0x00U, 0xE5ABU}, {0x00U, 0xE5F4U}, {0x00U, 0xE5F6U}, {0x00U, 0xE5FDU},
    // math transfer seam
    {0x01U, 0xC9C3U}, {0x01U, 0xC9D3U}, {0x00U, 0xE8A3U}, {0x00U, 0xE5ADU},
    {0x00U, 0xE5B8U}, {0x00U, 0xE5B2U}, {0x00U, 0xE5B4U}, {0x00U, 0xE5EFU},
    {0x00U, 0xE5B6U}, {0x00U, 0xE5FBU}, {0x00U, 0xE5A0U}, {0x00U, 0xE5A2U},
    // ranked timer seam
    {0x03U, 0x7E7EU}, {0x03U, 0x66A3U}, {0x03U, 0x7643U}, {0x03U, 0x7E6AU},
    {0x45U, 0x485DU}, {0x03U, 0x7E76U}, {0x03U, 0x7E73U}, {0x02U, 0x567FU},
    {0x03U, 0x66AAU}, {0x03U, 0x66B2U}, {0x00U, 0xE438U}, {0x03U, 0x66ADU},
    // eyeball countdown seam
    {0x02U, 0x4E32U}, {0x03U, 0x7C34U}, {0x02U, 0x432CU}, {0x02U, 0x4E24U},
    {0x45U, 0x4B85U}, {0x45U, 0x49E3U}, {0x02U, 0x4E29U}, {0x02U, 0x4E2DU},
    {0x03U, 0x7DE0U}, {0x02U, 0x4324U}, {0x02U, 0x431BU}, {0x02U, 0x4327U},
    // bumper sprite seam
    {0x03U, 0x7A71U}, {0x03U, 0x7A77U}, {0x03U, 0x7A7FU}, {0x03U, 0x7A82U},
    {0x03U, 0x7ADBU}, {0x03U, 0x7ADEU}, {0x03U, 0x76C5U}, {0x08U, 0x40DFU},
    {0x08U, 0x40F1U}, {0x08U, 0x40F6U}, {0x08U, 0x40FBU}, {0x08U, 0x4106U},
    // bonus countdown seam
    {0x05U, 0x7376U}, {0x05U, 0x7379U}, {0x05U, 0x7387U}, {0x05U, 0x738AU},
    {0x05U, 0x738FU}, {0x05U, 0x7390U}, {0x05U, 0x7393U}, {0x05U, 0x7396U},
    {0x05U, 0x7398U}, {0x02U, 0x4FC1U}, {0x02U, 0x4FE1U}, {0x04U, 0x5197U},
    // math gate seam
    {0x02U, 0x5CC5U}, {0x02U, 0x5CC8U}, {0x02U, 0x5CCBU}, {0x01U, 0xDA3EU},
    {0x00U, 0xE5AFU}, {0x00U, 0xE5ECU}, {0x00U, 0xE5F1U}, {0x00U, 0xE5F8U},
    {0x00U, 0xE824U}, {0x00U, 0xE5BBU}, {0x00U, 0xE5EBU}, {0x00U, 0xE836U},
    // ranked actor seam
    {0x02U, 0x45A9U}, {0x02U, 0x4571U}, {0x02U, 0x45A4U}, {0x02U, 0x59F4U},
    {0x02U, 0x4484U}, {0x02U, 0x56ABU}, {0x00U, 0xE60AU}, {0x03U, 0x61B2U},
    {0x02U, 0x4567U}, {0x02U, 0x456AU}, {0x02U, 0x5674U}, {0x02U, 0x5721U},
    {0x02U, 0x5F73U}, // tower source scheduler call
    // collision table call
    {0x02U, 0x441EU}, {0x02U, 0x4421U},
    // three blobs first movement prefix
    {0x03U, 0x66A3U}, {0x03U, 0x66A6U},
    // three blobs rng initialization
    {0x03U, 0x6678U}, {0x03U, 0x667BU}, {0x03U, 0x6697U}, {0x03U, 0x669AU},
    {0x03U, 0x66DCU}, // three blobs rng loop branch
    {0x03U, 0x66B5U}, // three blobs first movement continuation
    {0x03U, 0x66BFU}, // three blobs y expiry negation prefix
    {0x03U, 0x66E2U}, // three blobs alternate motion reset prefix
    // three blobs alternate motion arithmetic
    {0x03U, 0x670FU}, {0x03U, 0x6729U}, {0x03U, 0x6740U},
    // three blobs movement call seam
    {0x03U, 0x66D3U}, {0x03U, 0x66D6U}, {0x03U, 0x66D9U}, {0x03U, 0x670AU},
    {0x03U, 0x670CU}, {0x03U, 0x6726U}, {0x03U, 0x673DU},
    {0x03U, 0x66DEU}, // three blobs direct continuation
    {0x03U, 0x676BU}, // three blobs shared motion idle
    {0x03U, 0x67E5U}, // three blobs shared return
    // three blobs collision gate
    {0x03U, 0x67C0U}, {0x03U, 0x67CFU},
    {0x03U, 0x694AU}, // three blobs collision marker
    // fixed scheduler submission
    {0x03U, 0x7B4EU}, {0x03U, 0x7B5DU},
    {0x03U, 0x7B7AU}, // fixed marker gate
    // fixed marker submission
    {0x03U, 0x7BBBU}, {0x03U, 0x7BCDU},
    // fixed marker helper
    {0x03U, 0x7BE0U}, {0x03U, 0x7BE5U}, {0x03U, 0x7BE8U}, {0x03U, 0x7BE9U},
    {0x03U, 0x7BECU},
    // fixed marker outer segment
    {0x03U, 0x7BB1U}, {0x03U, 0x7BB5U}, {0x03U, 0x7BC3U}, {0x03U, 0x7BC7U},
    {0x03U, 0x7BD5U}, {0x03U, 0x7BD9U}, {0x03U, 0x7BDFU},
    {0x02U, 0x5955U}, // scaled word add
    {0x03U, 0x6966U}, // collision table pass
    {0x02U, 0x449BU}, // upper table parent idle gate
    {0x05U, 0x7356U}, // bonus hazard timer
    // actor scan preamble
    {0x01U, 0xC4DEU}, {0x01U, 0xC500U},
    // main session loop
    {0x01U, 0xC157U}, {0x01U, 0xC15AU}, {0x01U, 0xC15DU}, {0x01U, 0xC166U},
    {0x01U, 0xC169U}, {0x01U, 0xC16CU}, {0x01U, 0xC16FU}, {0x01U, 0xC17AU},
    {0x01U, 0xC185U}, {0x01U, 0xC190U},
    // bank one jump table
    {0x01U, 0xC012U}, {0x01U, 0xC018U}, {0x01U, 0xC021U}, {0x01U, 0xC024U},
    {0x01U, 0xC027U}, {0x01U, 0xC02AU}, {0x01U, 0xC03CU}, {0x01U, 0xC042U},
    {0x01U, 0xC05DU}, {0x01U, 0xC060U}, {0x01U, 0xC063U}, {0x01U, 0xC069U},
    {0x01U, 0xC06CU}, {0x01U, 0xC06FU}, {0x01U, 0xC081U}, {0x01U, 0xC087U},
    {0x01U, 0xC09FU}, {0x01U, 0xC0A2U},
    {0x09U, 0x60C7U}, // palette colour cycle
    {0x09U, 0x7237U}, // score panel poll
    // score animation
    {0x09U, 0x70C1U}, {0x09U, 0x70C4U}, {0x09U, 0x71EAU},
    {0x09U, 0x72FDU}, // score panel glyph
    // score panel redraw
    {0x09U, 0x727DU}, {0x09U, 0x72A2U}, {0x09U, 0x72BDU}, {0x09U, 0x72C5U},
    {0x01U, 0xC510U}, // actor scan run
    {0x01U, 0xC523U}, // actor scan advance
    {0x08U, 0x40C3U}, // sat tail clear
    {0x01U, 0xC52CU}, // actor dispatch vector
    {0x01U, 0xC606U}, // actor queue registration
    // sound timer driver
    {0x00U, 0xFC00U}, {0x00U, 0xFC82U}, {0x00U, 0xFD76U},
    // music sequencer
    {0x45U, 0x4000U}, {0x45U, 0x4003U}, {0x45U, 0x4006U}, {0x45U, 0x4009U},
    {0x45U, 0x4015U}, {0x45U, 0x4044U}, {0x45U, 0x404FU}, {0x45U, 0x409CU},
    {0x45U, 0x409FU}, {0x45U, 0x40A2U}, {0x45U, 0x413BU}, {0x45U, 0x4142U},
    {0x45U, 0x4143U}, {0x45U, 0x4154U}, {0x45U, 0x4169U}, {0x45U, 0x47E0U},
    {0x45U, 0x482EU}, {0x45U, 0x485EU}, {0x45U, 0x49E4U}, {0x45U, 0x4211U},
    {0x45U, 0x4228U}, {0x45U, 0x4389U}, {0x45U, 0x4434U}, {0x45U, 0x4368U},
    {0x45U, 0x46E2U}, {0x45U, 0x457AU}, {0x45U, 0x4667U}, {0x45U, 0x469BU},
    {0x45U, 0x4883U}, {0x45U, 0x48B4U}, {0x45U, 0x48F6U}, {0x45U, 0x4936U},
    {0x45U, 0x49A2U}, {0x45U, 0x49B6U}, {0x45U, 0x49F9U}, {0x45U, 0x4A16U},
    {0x45U, 0x4BCCU}, {0x45U, 0x4C42U}, {0x45U, 0x4B4EU},
    // bank8 export vectors
    {0x08U, 0x4000U}, {0x08U, 0x4003U}, {0x08U, 0x4006U}, {0x08U, 0x4009U},
    {0x08U, 0x400CU}, {0x08U, 0x400FU}, {0x08U, 0x4012U}, {0x08U, 0x4015U},
    {0x08U, 0x4018U}, {0x08U, 0x401BU}, {0x08U, 0x4030U},
    // palette shadow
    {0x00U, 0xE93CU}, {0x00U, 0xE9B6U}, {0x00U, 0xEAA7U}, {0x00U, 0xEAB0U},
    {0x00U, 0xEB30U}, {0x00U, 0xEB4DU}, {0x00U, 0xEBA5U},
    // setup screen palette stager
    {0x08U, 0x5961U}, {0x08U, 0x5977U}, {0x08U, 0x5982U},
    {0x08U, 0x5B01U}, // text tile write
    // music voice gate
    {0x45U, 0x4095U}, {0x45U, 0x4097U},
    // interrupt chain jump
    {0x00U, 0xE003U}, {0x00U, 0xFC03U}, {0x00U, 0xFC89U},
    // bit branch
    {0x01U, 0xCB68U}, {0x00U, 0xFA67U},
    // vram stream tail
    {0x00U, 0xE479U}, {0x00U, 0xE47CU},
    // upper scheduler return
    {0x08U, 0x5976U}, {0x02U, 0x4F75U}, {0x02U, 0x51D2U}, {0x02U, 0x51D5U},
    {0x45U, 0x4B85U}, {0x00U, 0xED85U}, {0x02U, 0x4F92U}, {0x09U, 0x704EU},
    {0x09U, 0x7186U}, {0x09U, 0x7099U}, {0x09U, 0x71C4U}, {0x09U, 0x7071U},
    {0x09U, 0x71A9U}, {0x03U, 0x7D81U}, {0x03U, 0x7E1FU}, {0x09U, 0x703CU},
    {0x09U, 0x7174U}, {0x00U, 0xE8E6U}, {0x01U, 0xC8E1U}, {0x00U, 0xECFDU},
    {0x00U, 0xED18U}, {0x00U, 0xED23U}, {0x00U, 0xED2CU}, {0x00U, 0xE333U},
    {0x01U, 0xDA3FU}, {0x09U, 0x70BBU}, {0x09U, 0x71E1U}, {0x01U, 0xC4C8U},
    {0x08U, 0x5A69U}, {0x01U, 0xC5EAU}, {0x01U, 0xC624U}, {0x01U, 0xC627U},
    {0x03U, 0x68BFU}, {0x01U, 0xC693U}, {0x01U, 0xC696U}, {0x01U, 0xC69BU},
    {0x01U, 0xC6A6U}, {0x01U, 0xC6BFU},
    {0x08U, 0x5A2EU}, // upper table actor tile schedule
    {0x08U, 0x5ABFU}, // upper table string record
    {0x08U, 0x5E42U}, // video update idle gate
    // boot work ram clear seams
    {0x08U, 0x584CU}, {0x08U, 0x584FU}, {0x08U, 0x5852U}, {0x08U, 0x5861U},
    // bank call wrappers
    {0x45U, 0x4B7BU}, {0x45U, 0x4B82U}, {0x00U, 0xE1FDU}, {0x00U, 0xE200U},
    {0x00U, 0xE20DU}, {0x00U, 0xE219U}, {0x00U, 0xE241U}, {0x00U, 0xE255U},
    {0x00U, 0xE262U}, {0x00U, 0xECC6U}, {0x00U, 0xECE2U}, {0x00U, 0xECF2U},
    {0x00U, 0xED58U}, {0x00U, 0xED75U},
    // vblank handler
    {0x00U, 0xE058U}, {0x00U, 0xE05FU}, {0x00U, 0xE0EBU}, {0x00U, 0xE09EU},
    {0x00U, 0xE114U}, {0x00U, 0xE117U}, {0x00U, 0xE123U}, {0x00U, 0xE135U},
    {0x00U, 0xE136U}, {0x00U, 0xE179U}, {0x00U, 0xE183U}, {0x00U, 0xE194U},
    {0x00U, 0xE195U},
    // vram upload
    {0x00U, 0xE271U}, {0x00U, 0xE34CU}, {0x00U, 0xE365U}, {0x00U, 0xE37EU},
    {0x00U, 0xEF2AU}, {0x00U, 0xE3C0U}, {0x00U, 0xE3D7U}, {0x00U, 0xE3FCU},
    {0x00U, 0xE403U}, {0x00U, 0xE41FU}, {0x00U, 0xE439U}, {0x00U, 0xE4E3U},
    {0x00U, 0xFA40U}, {0x00U, 0xFA6AU}, {0x00U, 0xFA6EU},
    // resident pattern loader
    {0x00U, 0xF768U}, {0x00U, 0xF778U}, {0x00U, 0xF781U},
    // tilemap upload
    {0x00U, 0xF261U}, {0x00U, 0xF2ABU}, {0x00U, 0xF301U}, {0x00U, 0xF330U},
    {0x00U, 0xF35FU}, {0x00U, 0xF3B5U}, {0x00U, 0xF3E4U}, {0x00U, 0xF3E7U},
    {0x00U, 0xF426U}, {0x00U, 0xF42AU},
    // scene selection controller
    {0x08U, 0x5A3EU}, {0x08U, 0x5A6AU}, {0x08U, 0x5A7AU}, {0x08U, 0x5A86U},
    // collision tail
    {0x01U, 0xCA84U}, {0x01U, 0xCAA1U}, {0x01U, 0xCAB6U}, {0x01U, 0xCAD3U},
    {0x01U, 0xCAE0U}, {0x01U, 0xCAEDU}, {0x01U, 0xCB04U}, {0x01U, 0xCB11U},
    {0x01U, 0xCB2AU}, {0x01U, 0xCB46U}, {0x01U, 0xCB65U}, {0x01U, 0xCB74U},
    {0x01U, 0xCB76U}, {0x01U, 0xCB79U}, {0x01U, 0xCB81U}, {0x01U, 0xCB8AU},
    {0x01U, 0xCBABU}, {0x01U, 0xCBB6U}, {0x01U, 0xCBC2U}, {0x01U, 0xCBCAU},
    {0x01U, 0xCBCFU}, {0x01U, 0xCD74U}, {0x01U, 0xCDF8U}, {0x01U, 0xCE22U},
    {0x01U, 0xCE4DU}, {0x01U, 0xCEB5U}, {0x01U, 0xCF33U}, {0x01U, 0xCF42U},
    {0x01U, 0xCF45U}, {0x01U, 0xCF53U}, {0x01U, 0xCF56U}, {0x01U, 0xCF60U},
    {0x01U, 0xCF63U}, {0x01U, 0xCF6AU}, {0x01U, 0xCF8CU},
    // sorcerer tick
    {0x02U, 0x586FU}, {0x02U, 0x5886U}, {0x02U, 0x5893U}, {0x02U, 0x58C2U},
    {0x02U, 0x58DDU},
    // actor geometry leaf
    {0x01U, 0xC8F3U}, {0x01U, 0xC969U}, {0x01U, 0xDAF8U},
    {0x01U, 0xC9DEU}, // range helper return
    // actor camera sweep
    {0x00U, 0xF4F2U}, {0x00U, 0xF523U}, {0x00U, 0xF545U}, {0x00U, 0xF54EU},
    // sat submission step
    {0x08U, 0x4036U}, {0x08U, 0x4063U}, {0x08U, 0x4075U}, {0x08U, 0x4088U},
    {0x08U, 0x4097U},
    {0x01U, 0xC51EU}, // actor dispatch call
    {0x01U, 0xC520U}, // actor queue call
    {0x01U, 0xC654U}, // actor queue return
    {0x01U, 0xCB48U}, // collision scan run
    {0x01U, 0xCB61U}, // collision scan candidate branch
    {0x01U, 0xCB6BU}, // collision scan advance
    {0x08U, 0x40DFU}, // sprite emit run
    {0x01U, 0xDA0EU},
    {0x02U, 0x51AFU},
    {0x01U, 0xC655U}, {0x01U, 0xC699U}, {0x01U, 0xC69EU}, {0x01U, 0xC6A9U},
};

// The scene selection controller, every address of it: deployable when
// every call is replaced, like the routines above.
constexpr std::uint8_t kProvenRangeBank = 0x08U;
constexpr std::uint16_t kProvenRangeFirst = 0x55A6U;
constexpr std::uint16_t kProvenRangeLast = 0x577AU;

struct ProvenCall {
    RoutineKey routine;
    std::uint64_t ordinal;
};

// Single calls of a routine above that are deployable on their own.
constexpr ProvenCall kProvenCalls[] = {
    // position integration
    {{0x00U, 0xF1B4U}, 1U},
    {{0x00U, 0xF1B4U}, 10U},
    {{0x00U, 0xF1B4U}, 17U},
    // motion damping
    {{0x01U, 0xDD09U}, 1U},
    {{0x01U, 0xDD09U}, 2U},
    {{0x01U, 0xDD09U}, 181U},
    // motion rotation
    {{0x01U, 0xD1A2U}, 23U},
    // collision response
    {{0x03U, 0x7FB1U}, 1U},
    {{0x03U, 0x7FB1U}, 3U},
    {{0x03U, 0x7FB1U}, 7U},
    // score accumulator
    {{0x00U, 0xF55CU}, 1U},
    // rng update
    {{0x00U, 0xE8B3U}, 1U},
    // unsigned multiply round
    {{0x00U, 0xE624U}, 1U},
    {{0x00U, 0xE624U}, 2U},
    // collision scan vertical reject
    {{0x01U, 0xCC1AU}, 19176U},
    // knight movement parent
    {{0x03U, 0x73A6U}, 2U},
    {{0x03U, 0x73A6U}, 4U},
    {{0x03U, 0x73A6U}, 16U},
    {{0x03U, 0x73A6U}, 58U},
    {{0x03U, 0x73A6U}, 60U},
    // collision table pass
    {{0x03U, 0x6966U}, 1U},
    {{0x03U, 0x6966U}, 2U},
    {{0x03U, 0x6966U}, 3U},
    {{0x03U, 0x6966U}, 4U},
    {{0x03U, 0x6966U}, 5U},
    {{0x03U, 0x6966U}, 6U},
    {{0x03U, 0x6966U}, 7U},
    {{0x03U, 0x6966U}, 8U},
    {{0x03U, 0x6966U}, 9U},
    {{0x03U, 0x6966U}, 10U},
    {{0x03U, 0x6966U}, 12U},
    {{0x03U, 0x6966U}, 14U},
    {{0x03U, 0x6966U}, 15U},
    {{0x03U, 0x6966U}, 16U},
    {{0x03U, 0x6966U}, 17U},
    {{0x03U, 0x6966U}, 18U},
    {{0x03U, 0x6966U}, 19U},
    {{0x03U, 0x6966U}, 20U},
    {{0x03U, 0x6966U}, 21U},
    {{0x03U, 0x6966U}, 22U},
    {{0x03U, 0x6966U}, 23U},
};

} // namespace

bool is_proven_replace_hook(const RoutineReplayRequest& request)
{
    const auto routine = request.routine;
    if (routine == kCachedPositionRestore) {
        return true;
    }
    if (!request.all_calls) {
        return std::any_of(
            std::begin(kProvenCalls),
            std::end(kProvenCalls),
            [&request](const ProvenCall& call) {
                return call.routine == request.routine
                    && call.ordinal == request.ordinal;
            });
    }
    if (routine.physical_bank == kProvenRangeBank
        && routine.logical_pc >= kProvenRangeFirst
        && routine.logical_pc <= kProvenRangeLast) {
        return true;
    }
    return std::find(
               std::begin(kProvenRoutines), std::end(kProvenRoutines), routine)
        != std::end(kProvenRoutines);
}

std::string unproven_replace_hook_message()
{
    return
        "Native replacement currently supports 00:F241, semantic 02:449B@all, semantic 05:7356@all, "
        "semantic "
        "00:F1B4@all, and the proven no-interrupt 00:F1B4@1, 00:F1B4@10, "
        "00:F1B4@17, semantic 01:D1A2@all, 01:D1A2@23, "
        "semantic 01:DD09@all, 01:DD09@1, 01:DD09@2, "
        "01:DD09@181, 03:6966@1, 03:6966@2, 03:6966@3, "
        "03:6966@4, 03:6966@5, 03:6966@6, 03:6966@7, "
        "03:6966@8, 03:6966@9, 03:6966@10, 03:6966@12, "
        "03:6966@14, 03:6966@15, 03:6966@16, 03:6966@17, "
        "03:6966@18, 03:6966@19, 03:6966@20, 03:6966@21, "
        "03:6966@22, 03:6966@23, semantic 03:6966@all, semantic "
        "03:7FB1@all, semantic 03:7F1A@all, semantic 03:7F57@all, "
        "semantic 07:4003@all, "
        "semantic 08:584C@all, semantic 08:584F@all, "
        "semantic 08:5852@all, semantic 08:5861@all, "
        "semantic 00:F54F@all, semantic 00:F55C@all, "
        "semantic 00:E8B3@all, semantic 00:E837@all, "
        "semantic 00:E905@all, semantic 00:E7D7@all, "
        "semantic 00:E728@all, "
        "semantic 00:E6DC@all, "
        "semantic 00:E569@all, "
        "semantic 00:E5BC@all, "
        "semantic 02:4FC4@all, "
        "semantic 00:E624@all, "
        "semantic 00:E65C@all, "
        "semantic 01:D09E@all, semantic 01:D612@all, "
        "semantic 01:D62E@all, "
        "semantic 01:CFD8@all, "
        "semantic 01:CDF8/CE22/CE4D/CEB5@all, semantic "
        "01:CF33/CF42/CF45/CF53/CF56/CF60/CF63/CF6A/CF8C@all, "
        "semantic 00:F4FC@all, semantic "
        "01:C574@all, semantic 01:D466@all, semantic 01:D4B3@all, "
        "semantic 01:D458@all, semantic 01:D48E@all, semantic 01:D491@all, "
        "semantic 01:D494@all, "
        "semantic 01:D4C6@all, semantic 01:D4C9@all, "
        "semantic 01:D4CC@all, "
        "semantic 01:CC1A@all, "
        "01:CC1A@19176, "
        "semantic 01:CC98@all, "
        "semantic 01:CCD5@all, "
        "semantic "
        "01:D500@all, semantic 01:D525/D52C/D53B/D54A/D54D/D550/D553/"
        "D556/D589/D594/D599/D5A9/D5B8/D5BB/D5BE/"
        "D5C1/D5C4/D5D8@all, "
        "semantic 01:D081@all, semantic 01:C5C9@all, "
        "semantic 01:C3CB@all, semantic 01:C49E@all, semantic 01:DD6E@all, "
        "semantic 01:DC95@all, "
        "semantic 01:C6C2/C6C5/C6D9/C6E6/C6E9/C6EB/C6F2/C6F5/C708@all, "
        "semantic 01:C721@all, semantic 01:C72E@all, semantic 01:C742@all, "
        "semantic 01:C74C@all, semantic 01:C759@all, semantic 01:C75C@all, "
        "semantic 01:C75E@all, semantic 01:C761@all, semantic 01:C768@all, "
        "semantic 01:C76B@all, semantic 01:C76E@all, "
        "semantic 00:F08E@all, semantic 00:F09A@all, semantic 00:F0A4@all, "
        "semantic 00:F0AE@all, semantic 00:F0B8@all, semantic 00:F0C0@all, "
        "semantic 00:F0FE@all, semantic 00:F104@all, semantic 00:F142@all, "
        "semantic 00:F148@all, semantic 00:F17E@all, "
        "semantic 01:C4DE@all, "
        "semantic 01:C157@all, semantic 01:C15A@all, semantic 01:C15D@all, "
        "semantic 01:C166@all, semantic 01:C169@all, semantic 01:C16C@all, "
        "semantic 01:C16F@all, semantic 01:C17A@all, semantic 01:C185@all, "
        "semantic 01:C190@all, "
        "semantic 01:C012@all, semantic 01:C018@all, semantic 01:C021@all, "
        "semantic 01:C024@all, semantic 01:C027@all, semantic 01:C02A@all, "
        "semantic 01:C03C@all, semantic 01:C042@all, semantic 01:C05D@all, "
        "semantic 01:C060@all, semantic 01:C063@all, semantic 01:C069@all, "
        "semantic 01:C06C@all, semantic 01:C06F@all, semantic 01:C081@all, "
        "semantic 01:C087@all, semantic 01:C09F@all, semantic 01:C0A2@all, "
        "semantic 09:60C7@all, "
        "semantic 09:70C1@all, semantic 09:70C4@all, "
        "semantic 09:71EA@all, semantic 09:7237@all, "
        "semantic 09:727D/72A2/72BD/72C5@all, "
        "semantic 09:72FD@all, "
        "semantic 01:C80A@all, semantic 01:C7F4@all, semantic 01:C7EF@all, semantic 01:C7F9@all, semantic 01:C7FC@all, semantic 01:C804@all, semantic 01:C812@all, semantic 01:C7EA@all, semantic 01:C7E5@all, "
        "semantic 02:427C@all, semantic 02:42A3@all, semantic 02:42B0@all, "
        "semantic 02:42EB@all, "
        "semantic 02:42F6@all, semantic 02:4345@all, "
        "semantic 02:4357@all, semantic 02:4371@all, "
        "semantic 02:4387@all, "
        "semantic 02:445B@all, "
        "semantic 02:570A@all, semantic 02:5674@all, "
        "semantic 02:5397@all, semantic 02:5408@all, semantic 02:53C8@all, semantic 02:53E4@all, semantic 02:5441@all, semantic 02:545D@all, semantic 02:53DD@all, semantic 02:5456@all, "
        "semantic 02:4DC8@all, semantic 02:4DD9@all, semantic 02:4DF7@all, semantic 02:4E24@all, semantic 02:4F3D@all, semantic 03:76C5@all, "
        "semantic 03:6876@all, semantic 03:68A1@all, semantic 02:5A31@all, semantic 02:5EE4@all, semantic 02:5EEC@all, semantic 03:664A@all, semantic 03:65BC@all, semantic 03:66DE@all, "
        "semantic 02:57AD@all, "
        "semantic 02:4FC1@all, "
        "semantic 02:4090@all, semantic 02:4096@all, semantic 02:4099@all, semantic 02:40A7@all, semantic 02:40F6@all, semantic 02:40FB@all, semantic 02:40FE@all, "
        "semantic 02:4A93@all, semantic 02:4AD2@all, "
        "semantic 02:4AE5@all, semantic 02:4AEB@all, "
        "semantic 02:4B3F@all, "
        "semantic 02:4BD5@all, semantic 02:4C0A@all, semantic 02:4C33@all, semantic 02:4CF1@all, semantic 02:4CFB@all, semantic 02:4D0B@all, "
        "semantic 02:5116@all, "
        "semantic 02:5159@all, "
        "semantic 02:518D@all, "
        "semantic 02:5197@all, "
        "semantic 02:5475@all, semantic 02:5481@all, "
        "semantic 02:5208@all, semantic 02:520B@all, "
        "semantic 02:5249@all, "
        "semantic 02:524C@all, "
        "semantic 02:525F@all, "
        "semantic 02:52E3@all, "
        "semantic 02:533A@all, "
        "semantic 02:533B@all, "
        "semantic 02:52B8@all, "
        "semantic 02:52C3@all, semantic 02:52C6@all, "
        "semantic 02:54B9@all, "
        "semantic 02:54D0@all, "
        "semantic 02:5500@all, "
        "semantic 02:5551@all, "
        "semantic 02:5568@all, "
        "semantic 02:5590@all, "
        "semantic 02:55F9@all, "
        "semantic 02:5604@all, "
        "semantic 02:561A@all, "
        "semantic 02:5625@all, "
        "semantic 02:5630@all, "
        "semantic 02:5A4B@all, "
        "semantic 02:5A6E@all, "
        "semantic 03:63F1@all, "
        "semantic 03:640A@all, "
        "semantic 03:6445@all, "
        "semantic 03:6294@all, semantic 03:62B8@all, semantic 03:62CB@all, "
        "semantic 03:62D4@all, semantic 03:62E2@all, "
        "semantic 03:74D8@all, "
        "semantic 03:767E@all, "
        "semantic 03:76FA@all, "
        "semantic 03:7726@all, "
        "semantic 03:6339@all, "
        "semantic 03:60C3@all, "
        "semantic 03:60F9@all, "
        "semantic 03:6136@all, "
        "semantic 03:60B3@all, "
        "semantic 03:7378@all, "
        "semantic 03:73A6@all, "
         "semantic 03:73D8@all, "
         "semantic 03:73FE@all, "
         "semantic 03:740A@all, "
         "semantic 03:7418@all, "
         "semantic 03:7420@all, "
         "semantic 02:41A0@all, "
         "semantic 02:41A8@all, "
         "semantic 02:41AD@all, "
         "semantic 02:41C2@all, "
         "semantic 02:41F5@all, "
         "semantic 02:420B@all, "
         "semantic 02:420E@all, "
         "semantic 02:4210@all, "
         "semantic 02:4212@all, "
         "semantic 02:4215@all, "
         "semantic 02:4217@all, "
         "semantic 02:421A@all, "
         "semantic 02:421C@all, "
         "semantic 02:421E@all, "
         "semantic 02:4222@all, "
         "semantic 02:4223@all, "
         "semantic 02:4226@all, "
         "semantic 02:4228@all, "
         "semantic 02:422B@all, "
         "semantic 02:422E@all, "
         "semantic 02:4230@all, "
         "semantic 02:4233@all, "
         "semantic 02:4235@all, "
         "semantic 02:4237@all, "
         "semantic 02:423B@all, "
         "semantic 02:423C@all, "
         "semantic 02:423F@all, "
         "semantic 02:4241@all, "
         "semantic 02:4244@all, "
         "semantic 02:4247@all, "
         "semantic 02:4249@all, "
         "semantic 02:424C@all, "
         "semantic 02:424D@all, "
         "semantic 02:4250@all, "
         "semantic 02:4253@all, "
         "semantic 02:4256@all, "
         "semantic 02:4258@all, "
         "semantic 02:425A@all, "
         "semantic 02:425D@all, "
         "semantic 02:425F@all, "
         "semantic 02:4262@all, "
         "semantic 02:4264@all, "
         "semantic 01:C024@all, "
         "semantic 01:C5AB@all, "
         "semantic 01:C5AD@all, "
         "semantic 01:C5B0@all, "
         "semantic 01:C5B2@all, "
         "semantic 01:C5B3@all, "
         "semantic 01:C5B5@all, "
         "semantic 01:C5B9@all, "
         "semantic 01:C5BB@all, "
         "semantic 01:C5BE@all, "
         "semantic 01:C5C0@all, "
         "semantic 01:C5C7@all, "
         "semantic 03:6490@all, semantic 03:651F@all, "
         "semantic 03:65A4@all, semantic 03:64D2/64DD/64ED@all, "
        "semantic 03:6054@all, "
        "semantic 02:5F22@all, "
        "semantic 02:5F4A@all, "
        "semantic 02:5F5B@all, "
        "semantic 02:5F78/5F7B/5F7E/5FD6@all, "
        "semantic 02:5F5F@all, "
        "semantic 02:5FDB@all, semantic 02:5FE6@all, "
        "semantic 03:7D85@all, semantic 03:7E23@all, "
        "semantic 03:7D4D@all, semantic 03:7DE9@all, "
        "semantic 03:7D6F@all, semantic 03:7E0D@all, "
        "semantic 03:7428@all, semantic 03:742B@all, "
        "semantic 03:742E@all, "
        "semantic 03:766C@all, "
        "semantic 03:76F7@all, "
        "semantic 03:7718@all, "
        "semantic 02:57E9@all, "
        "semantic 03:60E0@all, "
        "semantic 03:611C@all, "
        "semantic 03:60A6@all, "
        "semantic 03:78C7@all, "
        "semantic 03:789A@all, semantic 03:78A4@all, "
        "semantic 03:79DE@all, "
        "semantic 03:78F5@all, "
        "semantic 03:7912@all, "
        "semantic 03:7CAF@all, "
        "semantic 03:7CA7@all, "
        "semantic 03:7B27@all, "
        "semantic 03:7B4E@all, semantic 03:7B5D@all, "
        "semantic 03:7B7A@all, "
        "semantic 03:7BB1/7BB5/7BC3/7BC7/7BD5/7BD9/7BDF@all, "
        "semantic 03:7BBB@all, semantic 03:7BCD@all, "
        "semantic 03:7BE0@all, semantic 03:7BE5@all, "
        "semantic 03:7BE8@all, semantic 03:7BE9@all, "
        "semantic 03:7BEC@all, "
        "semantic 02:5955@all, "
        "semantic 02:5972@all, semantic 02:5975@all, semantic 02:5991@all, "
        "semantic 02:59A5@all, "
          "semantic 02:59C7@all, "
          "semantic 02:59E0@all, "
          "semantic 02:5A08@all, "
          "semantic 02:5A0B@all, "
          "semantic 02:5A1B@all, "
        "semantic 03:65BC@all, semantic 03:65C2@all, semantic 03:67B3@all, "
        "semantic 03:6678/667B/6697/669A@all, "
        "semantic 03:66A3/66B5@all, semantic 03:670F@all, "
        "semantic 03:6729@all, semantic 03:6740@all, and semantic "
        "03:676B@all, semantic 03:67E5@all, semantic 03:67C0/67CF@all, "
        "semantic 03:694A@all, "
        "semantic 03:7BF6/7C02/7C05@all, semantic "
        "03:7E87/7E8A/7EEE@all, semantic 03:7EFA/7F05@all, "
        "the proven 03:7FB1@1, 03:7FB1@3, 03:7FB1@7, 00:F55C@1, "
        "and 00:E8B3@1 calls.";
}

} // namespace devils_crush::app::host
