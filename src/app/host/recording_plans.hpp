// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "host_common.hpp"
#include "recording_inputs.hpp"

namespace devils_crush::app::host {

constexpr int kCanonicalRecordingFrames = 600;
constexpr int kGameplayRecordingFrames = 1800;
constexpr int kGameplayAlternateRecordingFrames = 3600;
constexpr int kGameplaySweepRecordingFrames = 3600;
constexpr int kGameplayCrossfireRecordingFrames = 3600;
constexpr int kGameplayOrbitRecordingFrames = 3600;
constexpr int kGameplayLanesRecordingFrames = 3600;
constexpr int kGameplayStaggerRecordingFrames = 3600;
constexpr int kGameplayPulseRecordingFrames = 3600;
constexpr int kGameplayRallyRecordingFrames = 3600;
constexpr int kGameplaySlalomRecordingFrames = 3600;
constexpr int kGameplayPendulumRecordingFrames = 3600;
constexpr int kGameplayNudgeRecordingFrames = 3600;
constexpr int kGameplaySwitchbackRecordingFrames = 3600;
constexpr int kGameplaySpiralRecordingFrames = 3600;
constexpr int kGameplayRicochetRecordingFrames = 3600;
constexpr int kGameplayVolleyRecordingFrames = 3600;
constexpr int kGameplayWhirlpoolRecordingFrames = 3600;
constexpr int kGameplayCascadeRecordingFrames = 3600;
constexpr int kGameplayHigharcRecordingFrames = 3600;
constexpr int kGameplayTwistRecordingFrames = 3600;
constexpr int kGameplaySidestepRecordingFrames = 3600;
constexpr int kGameplayWeaveRecordingFrames = 3600;
constexpr int kGameplayBankshotRecordingFrames = 3600;
constexpr int kGameplayPlayfieldRecordingFrames = 3600;
constexpr int kGameplayCornerRecordingFrames = 3600;
constexpr int kGameplayAlleyRecordingFrames = 3600;
constexpr int kGameplayReboundRecordingFrames = 3600;
constexpr int kGameplaySkewRecordingFrames = 3600;
constexpr int kGameplayHelixRecordingFrames = 3600;
constexpr int kGameplayVectorRecordingFrames = 3600;
constexpr int kGameplayRadialRecordingFrames = 3600;
constexpr int kStateCensusRecordingFrames = 3600;

inline const std::filesystem::path kCanonicalRecordingPath =
    std::filesystem::path("canonical-v1.csv");
inline const std::filesystem::path kGameplayRecordingPath =
    std::filesystem::path("gameplay-v1.csv");
inline const std::filesystem::path kGameplayAlternateRecordingPath =
    std::filesystem::path("gameplay-alternate-v1.csv");
inline const std::filesystem::path kGameplaySweepRecordingPath =
    std::filesystem::path("gameplay-sweep-v1.csv");
inline const std::filesystem::path kGameplayCrossfireRecordingPath =
    std::filesystem::path("gameplay-crossfire-v1.csv");
inline const std::filesystem::path kGameplayOrbitRecordingPath =
    std::filesystem::path("gameplay-orbit-v1.csv");
inline const std::filesystem::path kGameplayLanesRecordingPath =
    std::filesystem::path("gameplay-lanes-v1.csv");
inline const std::filesystem::path kGameplayStaggerRecordingPath =
    std::filesystem::path("gameplay-stagger-v1.csv");
inline const std::filesystem::path kGameplayPulseRecordingPath =
    std::filesystem::path("gameplay-pulse-v1.csv");
inline const std::filesystem::path kGameplayRallyRecordingPath =
    std::filesystem::path("gameplay-rally-v1.csv");
inline const std::filesystem::path kGameplaySlalomRecordingPath =
    std::filesystem::path("gameplay-slalom-v1.csv");
inline const std::filesystem::path kGameplayPendulumRecordingPath =
    std::filesystem::path("gameplay-pendulum-v1.csv");
inline const std::filesystem::path kGameplayNudgeRecordingPath =
    std::filesystem::path("gameplay-nudge-v1.csv");
inline const std::filesystem::path kGameplaySwitchbackRecordingPath =
    std::filesystem::path("gameplay-switchback-v1.csv");
inline const std::filesystem::path kGameplaySpiralRecordingPath =
    std::filesystem::path("gameplay-spiral-v1.csv");
inline const std::filesystem::path kGameplayRicochetRecordingPath =
    std::filesystem::path("gameplay-ricochet-v1.csv");
inline const std::filesystem::path kGameplayVolleyRecordingPath =
    std::filesystem::path("gameplay-volley-v1.csv");
inline const std::filesystem::path kGameplayWhirlpoolRecordingPath =
    std::filesystem::path("gameplay-whirlpool-v1.csv");
inline const std::filesystem::path kGameplayCascadeRecordingPath =
    std::filesystem::path("gameplay-cascade-v1.csv");
inline const std::filesystem::path kGameplayHigharcRecordingPath =
    std::filesystem::path("gameplay-higharc-v1.csv");
inline const std::filesystem::path kGameplayTwistRecordingPath =
    std::filesystem::path("gameplay-twist-v1.csv");
inline const std::filesystem::path kGameplaySidestepRecordingPath =
    std::filesystem::path("gameplay-sidestep-v1.csv");
inline const std::filesystem::path kGameplayWeaveRecordingPath =
    std::filesystem::path("gameplay-weave-v1.csv");
inline const std::filesystem::path kGameplayBankshotRecordingPath =
    std::filesystem::path("gameplay-bankshot-v1.csv");
inline const std::filesystem::path kGameplayPlayfieldRecordingPath =
    std::filesystem::path("gameplay-playfield-v1.csv");
inline const std::filesystem::path kGameplayCornerRecordingPath =
    std::filesystem::path("gameplay-corner-v1.csv");
inline const std::filesystem::path kGameplayAlleyRecordingPath =
    std::filesystem::path("gameplay-alley-v1.csv");
inline const std::filesystem::path kGameplayReboundRecordingPath =
    std::filesystem::path("gameplay-rebound-v1.csv");
inline const std::filesystem::path kGameplaySkewRecordingPath =
    std::filesystem::path("gameplay-skew-v1.csv");
inline const std::filesystem::path kGameplayHelixRecordingPath =
    std::filesystem::path("gameplay-helix-v1.csv");
inline const std::filesystem::path kGameplayVectorRecordingPath =
    std::filesystem::path("gameplay-vector-v1.csv");
inline const std::filesystem::path kGameplayRadialRecordingPath =
    std::filesystem::path("gameplay-radial-v1.csv");
inline const std::filesystem::path kStateCensusRecordingPath =
    std::filesystem::path("state-census-v1.csv");

using RecordingInputFunction = InputFrame (*)(int);

struct RecordingPlan {
    std::string_view format;
    std::string_view label;
    std::string_view input_script;
    int frame_count = 0;
    RecordingInputFunction input_for_frame = nullptr;
    std::span<const InputFrame> captured_inputs;
    std::span<const std::string> expected_work_ram_sha256s;
    // Interactive capture applies this host-side flipper setup before each
    // emulated frame. It changes the cartridge's per-piece input masks and
    // restores held flipper directions after the shooter gate, so it is part
    // of the replay contract even though it is not a pad-port bit.
    bool restore_interactive_flipper_host = false;
    bool restore_interactive_cabinet_host = false;
};

[[nodiscard]] InputFrame input_for_recording_plan_frame(
    const RecordingPlan& plan,
    const int frame_number);

constexpr RecordingPlan kCanonicalRecordingPlan{
    .format = "devils-crush-canonical-v1",
    .label = "Canonical",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421;two:480-481;left+one:540-541",
    .frame_count = kCanonicalRecordingFrames,
    .input_for_frame = &canonical_input_for_frame,
};

constexpr RecordingPlan kGameplayRecordingPlan{
    .format = "devils-crush-gameplay-v1",
    .label = "Gameplay",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,"
        "1000-1001,1100-1101,1200-1201,1300-1301,"
        "1400-1401,1500-1501,1600-1601,1700-1701;"
        "left:1000-1001,1100-1101,1200-1201,1300-1301,"
        "1400-1401,1500-1501,1600-1601,1700-1701;"
        "two:1050-1051,1250-1251,1450-1451,1650-1651",
    .frame_count = kGameplayRecordingFrames,
    .input_for_frame = &gameplay_input_for_frame,
};

constexpr RecordingPlan kGameplayAlternateRecordingPlan{
    .format = "devils-crush-gameplay-alternate-v1",
    .label = "Gameplay alternate",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1195,"
        "1235-1237+24n;"
        "left:1225-1227+24n;"
        "two:1425-1426,1825-1826,2225-2226,2625-2626,"
        "3025-3026,3425-3426",
    .frame_count = kGameplayAlternateRecordingFrames,
    .input_for_frame = &gameplay_alternate_input_for_frame,
};

constexpr RecordingPlan kGameplaySweepRecordingPlan{
    .format = "devils-crush-gameplay-sweep-v1",
    .label = "Gameplay sweep",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1215,1258-1259+40n;"
        "left:1240-1244+40n,1262-1266+40n;"
        "right:1250-1254+40n,1272-1279+40n;"
        "two:1375-1376+300n",
    .frame_count = kGameplaySweepRecordingFrames,
    .input_for_frame = &gameplay_sweep_input_for_frame,
};

constexpr RecordingPlan kGameplayCrossfireRecordingPlan{
    .format = "devils-crush-gameplay-crossfire-v1",
    .label = "Gameplay crossfire",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1225,1266-1267+32n,"
        "1282-1283+32n;"
        "left:1260-1275+32n,1284-1291+32n;"
        "right:1268-1283+32n;"
        "two:1510-1511,1910-1911,2310-2311,2710-2711,3110-3111,"
        "3510-3511",
    .frame_count = kGameplayCrossfireRecordingFrames,
    .input_for_frame = &gameplay_crossfire_input_for_frame,
};

constexpr RecordingPlan kGameplayOrbitRecordingPlan{
    .format = "devils-crush-gameplay-orbit-v1",
    .label = "Gameplay orbit",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1235,1268-1269+48n,"
        "1288-1289+48n;"
        "left:1260-1267+48n,1292-1295+48n;"
        "right:1272-1279+48n,1300-1307+48n;"
        "up:1264-1265+48n;right:1280-1281+48n;down:1296-1297+48n;"
        "two:1540-1541,2020-2021,2500-2501,2980-2981,3460-3461",
    .frame_count = kGameplayOrbitRecordingFrames,
    .input_for_frame = &gameplay_orbit_input_for_frame,
};

constexpr RecordingPlan kGameplayLanesRecordingPlan{
    .format = "devils-crush-gameplay-lanes-v1",
    .label = "Gameplay lanes",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1245,1310-1311+96n,"
        "1358-1359+96n;"
        "left:1280-1303+96n,1320-1321+96n;"
        "right:1328-1351+96n,1340-1341+96n;"
        "up:1292-1293+96n;down:1368-1369+96n;"
        "two:1460-1461,1940-1941,2420-2421,2900-2901,3380-3381",
    .frame_count = kGameplayLanesRecordingFrames,
    .input_for_frame = &gameplay_lanes_input_for_frame,
};

constexpr RecordingPlan kGameplayStaggerRecordingPlan{
    .format = "devils-crush-gameplay-stagger-v1",
    .label = "Gameplay stagger",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1255,1340-1341+144n,"
        "1420-1421+144n;"
        "left:1296-1315+144n;right:1380-1399+144n;"
        "up:1330-1331+144n;right:1364-1365+144n;down:1410-1411+144n;"
        "two:1580-1581,2180-2181,2780-2781,3380-3381",
    .frame_count = kGameplayStaggerRecordingFrames,
    .input_for_frame = &gameplay_stagger_input_for_frame,
};

constexpr RecordingPlan kGameplayPulseRecordingPlan{
    .format = "devils-crush-gameplay-pulse-v1",
    .label = "Gameplay pulse",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1270,1359-1360+180n,"
        "1464-1465+180n;"
        "left:1320-1323+180n,1374-1377+180n;"
        "right:1344-1347+180n,1434-1437+180n;"
        "up:1392-1393+180n;down:1479-1480+180n;"
        "two:1710-1711,2430-2431,3150-3151",
    .frame_count = kGameplayPulseRecordingFrames,
    .input_for_frame = &gameplay_pulse_input_for_frame,
};

constexpr RecordingPlan kGameplayRallyRecordingPlan{
    .format = "devils-crush-gameplay-rally-v1",
    .label = "Gameplay rally",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1290,1390-1391+137n,"
        "1446-1447+137n;"
        "left:1332-1340+137n,1403-1409+137n;"
        "right:1352-1383+137n;"
        "down:1344-1345+137n;up:1423-1424+137n;"
        "two:1680-1681,2365-2366,3050-3051",
    .frame_count = kGameplayRallyRecordingFrames,
    .input_for_frame = &gameplay_rally_input_for_frame,
};

constexpr RecordingPlan kGameplaySlalomRecordingPlan{
    .format = "devils-crush-gameplay-slalom-v1",
    .label = "Gameplay slalom",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1305,1392-1393+211n,"
        "1501-1502+211n;"
        "left:1350-1377+211n,1491-1492+211n;"
        "right:1381-1382+211n,1458-1485+211n;"
        "up:1436-1437+211n;"
        "two:1630-1631,2263-2264,2896-2897,3529-3530",
    .frame_count = kGameplaySlalomRecordingFrames,
    .input_for_frame = &gameplay_slalom_input_for_frame,
};

constexpr RecordingPlan kGameplayPendulumRecordingPlan{
    .format = "devils-crush-gameplay-pendulum-v1",
    .label = "Gameplay pendulum",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1320,1375-1376+257n,"
        "1545-1546+257n;"
        "left:1350-1364+257n,1521-1535+257n;"
        "right:1428-1442+257n,1564-1578+257n,1591-1592+257n;"
        "up:1393-1394+257n;down:1470-1471+257n;"
        "two:1764-1765,2535-2536,3306-3307",
    .frame_count = kGameplayPendulumRecordingFrames,
    .input_for_frame = &gameplay_pendulum_input_for_frame,
};

constexpr RecordingPlan kGameplayNudgeRecordingPlan{
    .format = "devils-crush-gameplay-nudge-v1",
    .label = "Gameplay nudge",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1335,1432-1433+313n,"
        "1624-1625+313n;"
        "left:1398-1403+313n,1588-1593+313n,1680-1681+313n;"
        "right:1456-1457+313n,1484-1489+313n,1652-1657+313n;"
        "up:1380-1381+313n;down:1536-1537+313n;"
        "two:1848-1849,2787-2788",
    .frame_count = kGameplayNudgeRecordingFrames,
    .input_for_frame = &gameplay_nudge_input_for_frame,
};

constexpr RecordingPlan kGameplaySwitchbackRecordingPlan{
    .format = "devils-crush-gameplay-switchback-v1",
    .label = "Gameplay switchback",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1410,1512-1513+233n,"
        "1636-1637+233n;"
        "left:1462-1497+233n,1652-1653+233n;"
        "right:1542-1543+233n,1582-1617+233n;"
        "down:1450-1451+233n;"
        "two:1733-1734,2432-2433,3131-3132",
    .frame_count = kGameplaySwitchbackRecordingFrames,
    .input_for_frame = &gameplay_switchback_input_for_frame,
};

constexpr RecordingPlan kGameplaySpiralRecordingPlan{
    .format = "devils-crush-gameplay-spiral-v1",
    .label = "Gameplay spiral",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1180,1250-1255+359n,"
        "1458-1463+359n;"
        "left:1218-1235+359n,1416-1427+359n,1499-1500+359n;"
        "right:1289-1306+359n,1495-1506+359n;"
        "up:1222-1223+359n;down:1420-1421+359n;"
        "two:1569-1570,2646-2647,3364-3365",
    .frame_count = kGameplaySpiralRecordingFrames,
    .input_for_frame = &gameplay_spiral_input_for_frame,
};

constexpr RecordingPlan kGameplayRicochetRecordingPlan{
    .format = "devils-crush-gameplay-ricochet-v1",
    .label = "Gameplay ricochet",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1160;"
        "left:1210-1215+419n,1463-1468+419n,1566-1567+419n;"
        "right:1307-1312+419n,1537-1542+419n,1346-1347+419n;"
        "up:1195-1196+419n;down:1424-1425+419n;"
        "two:1251-1252+419n,1385-1386+419n,1501-1502+419n,"
        "1581-1582+419n",
    .frame_count = kGameplayRicochetRecordingFrames,
    .input_for_frame = &gameplay_ricochet_input_for_frame,
};

constexpr RecordingPlan kGameplayVolleyRecordingPlan{
    .format = "devils-crush-gameplay-volley-v1",
    .label = "Gameplay volley",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1090,1135-1174+503n,"
        "1391-1432+503n;"
        "left:1197-1238+503n,1454-1499+503n,1617-1618+503n;"
        "up:1284-1285+503n;right:1367-1368+503n;"
        "down:1517-1518+503n;"
        "two:1262-1265+503n,1341-1344+503n,1535-1538+503n,"
        "1598-1601+503n",
    .frame_count = kGameplayVolleyRecordingFrames,
    .input_for_frame = &gameplay_volley_input_for_frame,
};

constexpr RecordingPlan kGameplayWhirlpoolRecordingPlan{
    .format = "devils-crush-gameplay-whirlpool-v1",
    .label = "Gameplay whirlpool",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1140,1342-1347+521n,"
        "1639-1644+521n;"
        "left:1197-1216+521n,1466-1485+521n,1681-1682+521n;"
        "right:1259-1282+521n,1553-1578+521n,1414-1415+521n;"
        "up:1234-1235+521n;down:1519-1520+521n;"
        "two:1310-1311+521n,1613-1614+521n",
    .frame_count = kGameplayWhirlpoolRecordingFrames,
    .input_for_frame = &gameplay_whirlpool_input_for_frame,
};

constexpr RecordingPlan kGameplayCascadeRecordingPlan{
    .format = "devils-crush-gameplay-cascade-v1",
    .label = "Gameplay cascade",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1064,1623-1628+607n;"
        "left:1195-1206+607n,1347-1348+607n;"
        "right:1414-1431+607n,1680-1681+607n;"
        "down:1142-1143+607n;up:1500-1501+607n;"
        "two:1289-1290+607n,1535-1536+607n",
    .frame_count = kGameplayCascadeRecordingFrames,
    .input_for_frame = &gameplay_cascade_input_for_frame,
};

constexpr RecordingPlan kGameplayHigharcRecordingPlan{
    .format = "devils-crush-gameplay-higharc-v1",
    .label = "Gameplay higharc",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1460,1768-1772+701n;"
        "left:1581-1588+701n,1704-1705+701n;"
        "right:1922-1931+701n,2136-2137+701n;"
        "up:1527-1528+701n;down:1857-1858+701n;"
        "two:1636-1637+701n,2061-2062+701n",
    .frame_count = kGameplayHigharcRecordingFrames,
    .input_for_frame = &gameplay_higharc_input_for_frame,
};

constexpr RecordingPlan kGameplayTwistRecordingPlan{
    .format = "devils-crush-gameplay-twist-v1",
    .label = "Gameplay twist",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1340,1923-1928+887n;"
        "left:1465-1488+887n,1618-1619+887n;"
        "right:1712-1713+887n,1931-1959+887n,2099-2100+887n;"
        "up:1414-1415+887n;down:1865-1866+887n;"
        "two:1582-1583+887n,2144-2145+887n",
    .frame_count = kGameplayTwistRecordingFrames,
    .input_for_frame = &gameplay_twist_input_for_frame,
};

constexpr RecordingPlan kGameplaySidestepRecordingPlan{
    .format = "devils-crush-gameplay-sidestep-v1",
    .label = "Gameplay sidestep",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1275,1732-1737+947n;"
        "left:1340-1341+947n,1366-1377+947n,1914-1931+947n;"
        "right:1545-1551+947n,1833-1834+947n,2086-2111+947n;"
        "two:1444-1445+947n,1669-1670+947n,2018-2019+947n,2199-2200+947n;"
        "up:1493-1494+947n;down:2237-2238+947n",
    .frame_count = kGameplaySidestepRecordingFrames,
    .input_for_frame = &gameplay_sidestep_input_for_frame,
};

constexpr RecordingPlan kGameplayWeaveRecordingPlan{
    .format = "devils-crush-gameplay-weave-v1",
    .label = "Gameplay weave",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1395,1958-1963+613n;"
        "left:1503-1524+613n;right:1799-1825+613n;"
        "up:1471-1472+613n;right:1658-1659+613n;"
        "down:1877-1878+613n;left:2048-2049+613n;"
        "two:1590-1591+613n,1734-1735+613n,1897-1898+613n,2030-2031+613n",
    .frame_count = kGameplayWeaveRecordingFrames,
    .input_for_frame = &gameplay_weave_input_for_frame,
};

constexpr RecordingPlan kGameplayBankshotRecordingPlan{
    .format = "devils-crush-gameplay-bankshot-v1",
    .label = "Gameplay bankshot",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1310,1887-1891+1091n;"
        "left:1420-1434+1091n,2094-2105+1091n;"
        "right:1657-1677+1091n,2255-2264+1091n;"
        "up:1392-1393+1091n;right:1574-1575+1091n;"
        "down:1843-1844+1091n;left:2178-2179+1091n;"
        "two:1519-1520+1091n,1799-1800+1091n,2023-2024+1091n,"
        "2345-2346+1091n",
    .frame_count = kGameplayBankshotRecordingFrames,
    .input_for_frame = &gameplay_bankshot_input_for_frame,
};

constexpr RecordingPlan kGameplayPlayfieldRecordingPlan{
    .format = "devils-crush-gameplay-playfield-v1",
    .label = "Gameplay playfield",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1525,2302-2309+1277n;"
        "left:1629-1686+1277n,2688-2689+1277n;"
        "right:2006-2045+1277n,1890-1891+1277n;"
        "up:1612-1613+1277n;right:1890-1891+1277n;"
        "down:2238-2239+1277n;left:2688-2689+1277n;"
        "two:1779-1780+1277n,2177-2178+1277n,2564-2565+1277n,"
        "2762-2763+1277n",
    .frame_count = kGameplayPlayfieldRecordingFrames,
    .input_for_frame = &gameplay_playfield_input_for_frame,
};

constexpr RecordingPlan kGameplayCornerRecordingPlan{
    .format = "devils-crush-gameplay-corner-v1",
    .label = "Gameplay corner",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1480,2169-2175+743n;"
        "right:1627-1646+743n,1794-1795+743n;"
        "left:2015-2051+743n,2261-2262+743n;"
        "down:1567-1568+743n;right:1794-1795+743n;"
        "up:1947-1948+743n;left:2261-2262+743n;"
        "two:1716-1717+743n,1872-1873+743n,2100-2101+743n,"
            "2242-2243+743n",
    .frame_count = kGameplayCornerRecordingFrames,
    .input_for_frame = &gameplay_corner_input_for_frame,
};

constexpr RecordingPlan kGameplayAlleyRecordingPlan{
    .format = "devils-crush-gameplay-alley-v1",
    .label = "Gameplay alley",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1580,2279-2285+811n;"
        "left:1709-1734+811n,2255-2273+811n,2428-2429+811n;"
        "right:2032-2055+811n,2117-2118+811n;"
        "up:1669-1670+811n;down:1861-1862+811n;"
        "two:1775-1776+811n,1924-1925+811n,2175-2176+811n,"
            "2364-2365+811n",
    .frame_count = kGameplayAlleyRecordingFrames,
    .input_for_frame = &gameplay_alley_input_for_frame,
};

constexpr RecordingPlan kGameplayReboundRecordingPlan{
    .format = "devils-crush-gameplay-rebound-v1",
    .label = "Gameplay rebound",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1235,1791-1797+1009n;"
        "right:1342-1357+1009n,1704-1705+1009n,1974-2025+1009n;"
        "left:1520-1544+1009n,2129-2130+1009n;"
        "up:1325-1326+1009n;down:1483-1484+1009n;"
        "two:1435-1436+1009n,1656-1657+1009n,1884-1885+1009n,"
            "2196-2197+1009n",
    .frame_count = kGameplayReboundRecordingFrames,
    .input_for_frame = &gameplay_rebound_input_for_frame,
};

constexpr RecordingPlan kGameplaySkewRecordingPlan{
    .format = "devils-crush-gameplay-skew-v1",
    .label = "Gameplay skew",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1400,2037-2043+1201n;"
        "left:1540-1582+1201n,2313-2334+1201n,2457-2458+1201n;"
        "right:1702-1703+1201n,1821-1843+1201n;"
        "up:1493-1494+1201n;down:2081-2082+1201n;"
        "two:1615-1616+1201n,1909-1910+1201n,2164-2165+1201n,"
            "2518-2519+1201n",
    .frame_count = kGameplaySkewRecordingFrames,
    .input_for_frame = &gameplay_skew_input_for_frame,
};

constexpr RecordingPlan kGameplayHelixRecordingPlan{
    .format = "devils-crush-gameplay-helix-v1",
    .label = "Gameplay helix",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1515,2120-2126+1237n;"
        "right:1636-1678+1237n,2350-2378+1237n,2698-2699+1237n;"
        "left:1831-1832+1237n,1890-1917+1237n;"
        "up:1610-1611+1237n;down:2172-2173+1237n;"
        "two:1739-1740+1237n,1994-1995+1237n,2260-2261+1237n,"
            "2587-2588+1237n",
    .frame_count = kGameplayHelixRecordingFrames,
    .input_for_frame = &gameplay_helix_input_for_frame,
};

constexpr RecordingPlan kGameplayVectorRecordingPlan{
    .format = "devils-crush-gameplay-vector-v1",
    .label = "Gameplay vector",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1330,1985-1991+1271n;"
        "right:1480-1511+1271n,2214-2227+1271n,2556-2557+1271n;"
        "left:1676-1677+1271n,1772-1794+1271n;"
        "up:1435-1436+1271n;down:2030-2031+1271n;"
        "two:1570-1571+1271n,1845-1846+1271n,2114-2115+1271n,"
            "2436-2437+1271n",
    .frame_count = kGameplayVectorRecordingFrames,
    .input_for_frame = &gameplay_vector_input_for_frame,
};

constexpr RecordingPlan kGameplayRadialRecordingPlan{
    .format = "devils-crush-gameplay-radial-v1",
    .label = "Gameplay radial",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,1000-1445,2117-2123+1301n;"
        "left:1570-1592+1301n,2329-2347+1301n,2737-2738+1301n;"
        "right:1744-1745+1301n,1836-1864+1301n;"
        "up:1539-1540+1301n;down:2045-2046+1301n;"
        "two:1662-1663+1301n,1971-1972+1301n,2228-2229+1301n,"
            "2608-2609+1301n",
    .frame_count = kGameplayRadialRecordingFrames,
    .input_for_frame = &gameplay_radial_input_for_frame,
};

constexpr RecordingPlan kStateCensusRecordingPlan{
    .format = "devils-crush-state-census-v1",
    .label = "State census",
    .input_script =
        "run:180-181,300-301;"
        "one:420-421,700-701,850-880,"
        "1000-1180,1265-1266+30n;"
        "left:1250-1251+30n;"
        "two:1450-1451,2250-2251,3050-3051",
    .frame_count = kStateCensusRecordingFrames,
    .input_for_frame = &state_census_input_for_frame,
};

} // namespace devils_crush::app::host
