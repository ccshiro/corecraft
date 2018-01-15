/* Copyright (C) 2014 Corecraft <https://www.worldofcorecraft.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "alterac_valley.h"
#include "precompiled.h"

#define MAX_UNITS
static float horde_sf_grp[MAX_UNITS][4] = {{-305.6, -126.8, 15.4, 1.0},
    {-306.8, -128.4, 15.1, 1.0}, {-304.3, -128.1, 15.2, 1.0},
    {-305.3, -129.5, 15.0, 1.0}, {-302.3, -129.4, 15.0, 1.0},
    {-303.7, -130.7, 14.8, 1.0}, {-300.9, -130.3, 14.8, 1.0},
    {-302.1, -131.8, 14.6, 1.0}, {-299.0, -131.4, 14.5, 1.0},
    {-300.5, -132.9, 14.4, 1.0}};

static DynamicWaypoint horde_commander_start(-311.9, -144.3, 12.1, 4.3);

static float horde_field_grp[MAX_UNITS][4] = {{-307.6, -143.9, 12.2, 4.3},
    {-307.0, -142.1, 12.6, 4.3}, {-309.2, -143.2, 12.4, 4.3},
    {-308.7, -141.3, 12.8, 4.3}, {-311.0, -142.4, 12.5, 4.3},
    {-310.3, -140.6, 12.9, 4.3}, {-312.8, -141.3, 12.7, 4.3},
    {-312.0, -139.8, 13.0, 4.3}, {-314.2, -140.6, 12.8, 4.3},
    {-313.5, -139.1, 13.1, 4.3}};

static float ally_sf_grp[MAX_UNITS][4] = {{-240.0, -393.0, 12.0, 1.3},
    {-240.7, -395.8, 12.6, 1.3}, {-237.6, -393.9, 12.0, 1.3},
    {-238.5, -396.5, 12.5, 1.3}, {-235.4, -394.4, 12.0, 1.3},
    {-236.2, -397.0, 12.3, 1.3}, {-232.8, -394.9, 11.9, 1.3},
    {-233.7, -397.7, 12.2, 1.3}, {-230.5, -395.7, 11.9, 1.3},
    {-231.3, -398.3, 12.1, 1.3}};

static DynamicWaypoint ally_commander_start(-278.6, -412.5, 16.5, 2.4);

static float ally_field_grp[MAX_UNITS][4] = {{-279.0, -418.1, 17.4, 2.4},
    {-277.0, -419.9, 17.6, 2.4}, {-275.7, -418.3, 17.3, 2.4},
    {-277.7, -416.5, 17.1, 2.4}, {-276.4, -415.0, 16.9, 2.4},
    {-274.3, -416.9, 17.2, 2.4}, {-272.8, -415.5, 17.2, 2.4},
    {-275.2, -413.5, 17.0, 2.4}, {-273.5, -411.9, 17.1, 2.4},
    {-271.4, -413.3, 17.3, 2.4}};

/* WAYPOINT PATHS */
static DynamicWaypoint ally_sf_path[] = {
    DynamicWaypoint(-236.3, -395.9, 12.2, 1.3, 0, true),
    DynamicWaypoint(-236.4, -397.5, 12.4, 1.1, 0, true),
    DynamicWaypoint(-231.4, -381.3, 11.3, 1.3, 0, true),
    DynamicWaypoint(-226.4, -360.9, 8.2, 1.3, 0, true),
    DynamicWaypoint(-222.0, -342.7, 8.5, 1.3, 0, true),
    DynamicWaypoint(-209.8, -325.6, 8.2, 1.0, 0, true),
    DynamicWaypoint(-199.1, -308.9, 6.7, 1.0, 0, true),
    DynamicWaypoint(-187.4, -290.7, 6.7, 1.0, 0, true),
    DynamicWaypoint(-172.6, -272.0, 8.2, 0.9, 0, true),
    DynamicWaypoint(-158.5, -255.6, 8.3, 0.9, 0, true),
    DynamicWaypoint(-157.2, -237.6, 7.8, 1.4, 0, true),
    DynamicWaypoint(-158.6, -223.7, 13.9, 1.6, 0, true),
    DynamicWaypoint(-158.4, -210.2, 23.7, 1.6, 0, true),
    DynamicWaypoint(-159.8, -196.9, 30.6, 1.7, 0, true),
    DynamicWaypoint(-163.7, -185.8, 37.3, 1.9, 0, true),
    DynamicWaypoint(-168.3, -172.0, 47.2, 1.9, 0, true),
    DynamicWaypoint(-173.3, -162.1, 56.1, 2.1, 0, true),
    DynamicWaypoint(-179.9, -154.7, 62.5, 2.3, 0, true),
    DynamicWaypoint(-188.0, -145.5, 70.3, 2.3, 0, true),
    DynamicWaypoint(-195.6, -133.7, 76.8, 2.1, 0, true),
    DynamicWaypoint(-204.6, -117.4, 78.5, 2.1, 0, true),
    DynamicWaypoint(-219.1, -99.1, 79.1, 2.3, 0, true),
    DynamicWaypoint(-226.9, -92.0, 74.4, 2.4, 0, true),
    DynamicWaypoint(-234.8, -86.0, 66.1, 2.5, 0, true),
    DynamicWaypoint(-246.3, -80.3, 59.8, 2.8, 0, true),
    DynamicWaypoint(-258.4, -81.7, 55.0, 3.3, 0, true),
    DynamicWaypoint(-268.0, -90.3, 46.8, 3.8, 0, true),
    DynamicWaypoint(-278.2, -100.6, 36.3, 3.9, 0, true),
    DynamicWaypoint(-286.4, -109.0, 27.7, 3.9, 0, true),
    DynamicWaypoint(-296.2, -119.0, 18.3, 3.9, 0, true),
    DynamicWaypoint(-305.9, -133.6, 14.3, 4.1, 0, true),
    DynamicWaypoint(-325.1, -162.9, 9.3, 4.1, 0, true),
    DynamicWaypoint(-331.0, -170.0, 9.3, 3.7, 0, true),
    DynamicWaypoint(-365.5, -189.5, 12.4, 3.7, 0, true),
    DynamicWaypoint(-367.7, -218.1, 13.3, 4.6, 0, true),
    DynamicWaypoint(-378.0, -249.4, 13.6, 4.2, 0, true),
    DynamicWaypoint(-402.4, -270.2, 14.3, 3.8, 0, true),
    DynamicWaypoint(-436.2, -276.7, 20.7, 3.3, 0, true),
    DynamicWaypoint(-466.9, -280.6, 23.6, 3.3, 0, true),
    DynamicWaypoint(-489.5, -286.5, 28.8, 3.5, 0, true),
    DynamicWaypoint(-504.8, -312.7, 31.9, 4.2, 0, true),
    DynamicWaypoint(-523.6, -338.0, 34.5, 4.0, 0, true),
    DynamicWaypoint(-547.8, -334.6, 38.0, 3.0, 0, true),
    DynamicWaypoint(-573.2, -318.0, 44.3, 2.6, 0, true),
    DynamicWaypoint(-595.3, -323.4, 49.9, 3.4, 0, true),
    DynamicWaypoint(-606.8, -333.3, 52.5, 3.9, 0, true),
    DynamicWaypoint(-617.3, -345.6, 55.1, 4.0, 0, true),
    DynamicWaypoint(-621.8, -364.9, 56.7, 4.5, 0, true),
    DynamicWaypoint(-624.9, -383.3, 58.2, 4.5, 0, true),
    DynamicWaypoint(-639.9, -393.5, 59.7, 3.7, 0, true),
    DynamicWaypoint(-658.1, -387.2, 62.8, 2.7, 0, true),
    DynamicWaypoint(-676.8, -377.5, 65.6, 2.6, 0, true),
    DynamicWaypoint(-694.2, -368.0, 66.0, 2.6, 0, true),
    DynamicWaypoint(-711.5, -363.1, 66.8, 3.1, 0, true),
    DynamicWaypoint(-715.3, -381.4, 67.4, 4.5, 0, true),
    DynamicWaypoint(-719.4, -402.0, 67.6, 4.5, 0, true),
    DynamicWaypoint(-731.2, -414.9, 67.6, 4.0, 0, true),
    DynamicWaypoint(-751.0, -424.9, 66.5, 3.6, 0, true),
    DynamicWaypoint(-777.0, -431.7, 61.8, 3.4, 0, true),
    DynamicWaypoint(-796.5, -445.6, 56.5, 3.7, 0, true),
    DynamicWaypoint(-816.0, -442.3, 54.5, 2.7, 0, true),
    DynamicWaypoint(-826.9, -424.4, 52.7, 2.0, 0, true),
    DynamicWaypoint(-837.3, -403.4, 51.6, 2.0, 0, true),
    DynamicWaypoint(-854.0, -389.9, 49.8, 2.5, 0, true),
    DynamicWaypoint(-875.5, -387.9, 48.6, 3.1, 0, true),
    DynamicWaypoint(-896.5, -379.2, 48.8, 2.7, 0, true),
    DynamicWaypoint(-920.1, -382.0, 49.5, 3.4, 0, true),
    DynamicWaypoint(-942.9, -389.3, 48.6, 3.5, 0, true),
    DynamicWaypoint(-967.1, -395.9, 49.2, 3.4, 0, true),
    DynamicWaypoint(-994.3, -399.0, 50.1, 3.2, 0, true),
    DynamicWaypoint(-1018.5, -395.3, 50.7, 2.9, 0, true),
    DynamicWaypoint(-1047.2, -382.8, 51.0, 2.4, 0, true),
    DynamicWaypoint(-1067.5, -362.8, 51.4, 2.5, 0, true),
    DynamicWaypoint(-1092.0, -364.3, 51.4, 3.2, 0, true),
    DynamicWaypoint(-1119.0, -359.7, 51.5, 2.9, 0, true),
    DynamicWaypoint(-1138.7, -350.8, 51.2, 2.7, 0, true),
    DynamicWaypoint(-1164.8, -352.9, 51.8, 3.3, 0, true),
    DynamicWaypoint(-1188.3, -363.3, 52.3, 3.5, 0, true),
    DynamicWaypoint(-1213.8, -366.1, 56.4, 3.2, 0, true),
    DynamicWaypoint(-1242.4, -362.5, 59.7, 2.8, 0, true),
    DynamicWaypoint(-1250.9, -346.1, 59.4, 2.0, 0, true),
    DynamicWaypoint(-1238.9, -326.1, 60.1, 1.0, 0, true),
    DynamicWaypoint(-1225.0, -306.6, 65.4, 0.9, 0, true),
    DynamicWaypoint(-1209.8, -289.6, 71.6, 0.8, 0, true),
    DynamicWaypoint(-1197.6, -268.4, 72.4, 1.1, 0, true),
    DynamicWaypoint(-1211.5, -253.0, 72.7, 2.5, 0, true),
    DynamicWaypoint(-1237.6, -250.2, 73.3, 3.2, 0, true),
    DynamicWaypoint(-1258.4, -272.3, 73.0, 4.0, 0, true),
    DynamicWaypoint(-1271.8, -282.7, 80.6, 3.8, 0, true),
    DynamicWaypoint(-1280.2, -288.0, 87.1, 3.7, 0, true),
    DynamicWaypoint(-1295.0, -290.9, 90.5, 3.2, 0, true),
    DynamicWaypoint(-1322.4, -290.6, 90.6, 3.1, 0, true),
    DynamicWaypoint(-1347.5, -289.6, 90.9, 3.1, 0, true),
    DynamicWaypoint(-1352.3, -277.7, 95.1, 1.9, 0, true),
    DynamicWaypoint(-1357.0, -263.9, 99.3, 1.9, 0, true),
    DynamicWaypoint(-1362.0, -247.7, 99.4, 1.9, 0, true),
    DynamicWaypoint(-1380.5, -245.0, 99.4, 2.9, 0, true),
    DynamicWaypoint(-1379.6, -226.9, 99.4, 1.3, 0, true),
    DynamicWaypoint(-1371.0, -220.0, 98.4, 0.7, 0, true)};

static DynamicWaypoint ally_field_path[] = {
    DynamicWaypoint(-276.2, -414.7, 16.9, 2.3, 0, true),
    DynamicWaypoint(-290.7, -400.1, 12.5, 2.4, 0, true),
    DynamicWaypoint(-305.3, -379.7, 6.8, 2.2, 0, true),
    DynamicWaypoint(-302.6, -349.1, 6.7, 1.3, 0, true),
    DynamicWaypoint(-278.8, -343.6, 6.7, 0.2, 0, true),
    DynamicWaypoint(-252.7, -333.4, 6.7, 0.5, 0, true),
    DynamicWaypoint(-223.6, -316.2, 6.7, 0.5, 0, true),
    DynamicWaypoint(-195.5, -302.0, 6.7, 0.5, 0, true),
    DynamicWaypoint(-196.3, -284.6, 6.7, 1.6, 0, true),
    DynamicWaypoint(-197.3, -256.0, 6.7, 1.6, 0, true),
    DynamicWaypoint(-203.5, -228.7, 6.7, 1.9, 0, true),
    DynamicWaypoint(-225.6, -214.5, 6.7, 2.8, 0, true),
    DynamicWaypoint(-246.5, -221.7, 6.7, 3.6, 0, true),
    DynamicWaypoint(-270.3, -235.4, 8.2, 3.7, 0, true),
    DynamicWaypoint(-298.1, -251.5, 7.5, 3.7, 0, true),
    DynamicWaypoint(-324.9, -267.0, 7.2, 3.7, 0, true),
    DynamicWaypoint(-352.2, -282.8, 9.8, 3.7, 0, true),
    DynamicWaypoint(-375.5, -285.0, 11.7, 3.1, 0, true),
    DynamicWaypoint(-397.1, -280.0, 13.3, 2.9, 0, true),
    DynamicWaypoint(-412.4, -276.1, 17.0, 2.9, 0, true),
    DynamicWaypoint(-438.6, -274.3, 20.4, 3.1, 0, true),
    DynamicWaypoint(-458.9, -279.8, 22.5, 3.4, 0, true),
    DynamicWaypoint(-479.7, -282.5, 26.5, 3.3, 0, true),
    DynamicWaypoint(-496.2, -289.6, 30.3, 3.7, 0, true),
    DynamicWaypoint(-504.9, -308.0, 31.7, 4.2, 0, true),
    DynamicWaypoint(-515.8, -328.0, 33.5, 4.2, 0, true),
    DynamicWaypoint(-524.1, -340.6, 34.6, 3.1, 0, true),
    DynamicWaypoint(-543.3, -333.6, 37.7, 2.8, 0, true),
    DynamicWaypoint(-564.2, -323.2, 41.0, 2.7, 0, true),
    DynamicWaypoint(-578.6, -317.1, 46.0, 2.8, 0, true),
    DynamicWaypoint(-595.9, -322.5, 50.1, 3.4, 0, true),
    DynamicWaypoint(-607.5, -332.7, 52.6, 3.9, 0, true),
    DynamicWaypoint(-617.4, -346.7, 55.2, 4.1, 0, true),
    DynamicWaypoint(-623.9, -370.7, 57.0, 4.6, 0, true),
    DynamicWaypoint(-628.0, -395.4, 59.0, 4.5, 0, true),
    DynamicWaypoint(-640.8, -391.3, 59.8, 2.8, 0, true),
    DynamicWaypoint(-659.7, -385.1, 63.2, 2.8, 0, true),
    DynamicWaypoint(-681.9, -374.9, 65.6, 2.7, 0, true),
    DynamicWaypoint(-709.8, -361.5, 66.6, 2.7, 0, true),
    DynamicWaypoint(-713.7, -376.1, 67.2, 4.6, 0, true),
    DynamicWaypoint(-718.6, -399.4, 67.6, 4.5, 0, true),
    DynamicWaypoint(-732.1, -416.9, 67.6, 4.0, 0, true),
    DynamicWaypoint(-757.3, -427.6, 65.5, 3.5, 0, true),
    DynamicWaypoint(-784.4, -436.9, 59.5, 3.5, 0, true),
    DynamicWaypoint(-811.1, -449.9, 54.9, 3.5, 0, true),
    DynamicWaypoint(-822.7, -432.4, 53.1, 2.1, 0, true),
    DynamicWaypoint(-839.5, -400.3, 51.4, 2.0, 0, true),
    DynamicWaypoint(-858.8, -390.7, 49.3, 2.7, 0, true),
    DynamicWaypoint(-884.7, -383.7, 48.6, 2.7, 0, true),
    DynamicWaypoint(-916.2, -383.4, 49.3, 3.2, 0, true),
    DynamicWaypoint(-947.7, -392.0, 48.5, 3.4, 0, true),
    DynamicWaypoint(-977.0, -396.9, 49.4, 3.3, 0, true),
    DynamicWaypoint(-1012.6, -396.7, 50.8, 3.1, 0, true),
    DynamicWaypoint(-1048.2, -379.4, 51.2, 2.7, 0, true),
    DynamicWaypoint(-1072.0, -362.5, 51.4, 3.0, 0, true),
    DynamicWaypoint(-1101.7, -364.3, 51.5, 3.2, 0, true),
    DynamicWaypoint(-1137.2, -350.8, 51.3, 2.8, 0, true),
    DynamicWaypoint(-1163.9, -353.0, 51.8, 3.2, 0, true),
    DynamicWaypoint(-1191.2, -363.4, 52.6, 3.5, 0, true),
    DynamicWaypoint(-1217.2, -367.0, 56.9, 3.2, 0, true),
    DynamicWaypoint(-1241.1, -362.1, 59.8, 2.7, 0, true),
    DynamicWaypoint(-1245.7, -338.9, 59.3, 1.5, 0, true),
    DynamicWaypoint(-1228.7, -313.8, 62.6, 0.9, 0, true),
    DynamicWaypoint(-1214.0, -297.1, 70.2, 0.8, 0, true),
    DynamicWaypoint(-1197.7, -271.7, 72.3, 1.0, 0, true),
    DynamicWaypoint(-1207.6, -254.6, 72.5, 2.3, 0, true),
    DynamicWaypoint(-1237.1, -250.3, 73.3, 3.0, 0, true),
    DynamicWaypoint(-1246.2, -257.0, 73.3, 4.1, 0, true),
    DynamicWaypoint(-1258.1, -272.9, 73.1, 4.1, 0, true),
    DynamicWaypoint(-1270.0, -284.0, 80.0, 3.7, 0, true),
    DynamicWaypoint(-1284.9, -289.1, 88.7, 3.4, 0, true),
    DynamicWaypoint(-1306.4, -291.3, 90.7, 3.2, 0, true),
    DynamicWaypoint(-1336.8, -291.1, 90.9, 3.1, 0, true),
    DynamicWaypoint(-1348.9, -285.5, 91.2, 2.0, 0, true),
    DynamicWaypoint(-1354.8, -268.4, 98.3, 1.9, 0, true),
    DynamicWaypoint(-1361.6, -246.7, 99.4, 1.9, 0, true),
    DynamicWaypoint(-1379.1, -245.9, 99.4, 2.5, 0, true),
    DynamicWaypoint(-1380.3, -237.2, 99.4, 1.7, 0, true),
    DynamicWaypoint(-1371.1, -221.0, 98.4, 1.1, 0, true)};

static DynamicWaypoint horde_sf_path[] = {
    DynamicWaypoint(-303.2, -129.9, 14.9, 0.9, 0, true),
    DynamicWaypoint(-295.4, -116.9, 19.6, 0.7, 0, true),
    DynamicWaypoint(-288.6, -111.4, 25.5, 0.7, 0, true),
    DynamicWaypoint(-281.5, -104.4, 32.5, 0.8, 0, true),
    DynamicWaypoint(-274.2, -96.0, 41.1, 0.9, 0, true),
    DynamicWaypoint(-266.9, -89.3, 47.9, 0.7, 0, true),
    DynamicWaypoint(-259.3, -83.8, 53.6, 0.6, 0, true),
    DynamicWaypoint(-250.8, -79.9, 58.1, 0.4, 0, true),
    DynamicWaypoint(-243.3, -82.9, 60.9, 5.8, 0, true),
    DynamicWaypoint(-233.2, -88.8, 68.9, 5.7, 0, true),
    DynamicWaypoint(-225.8, -94.5, 75.8, 5.6, 0, true),
    DynamicWaypoint(-219.2, -101.1, 79.3, 5.5, 0, true),
    DynamicWaypoint(-207.5, -116.6, 78.7, 5.4, 0, true),
    DynamicWaypoint(-194.4, -136.6, 75.9, 5.2, 0, true),
    DynamicWaypoint(-185.7, -148.3, 68.2, 5.4, 0, true),
    DynamicWaypoint(-175.5, -160.4, 57.8, 5.4, 0, true),
    DynamicWaypoint(-168.1, -173.5, 46.0, 5.2, 0, true),
    DynamicWaypoint(-161.7, -189.9, 34.5, 5.1, 0, true),
    DynamicWaypoint(-158.9, -207.7, 25.2, 4.8, 0, true),
    DynamicWaypoint(-157.0, -224.0, 13.9, 4.8, 0, true),
    DynamicWaypoint(-153.7, -245.4, 6.9, 5.0, 0, true),
    DynamicWaypoint(-136.2, -254.7, 6.9, 5.8, 0, true),
    DynamicWaypoint(-107.6, -260.2, 6.6, 6.1, 0, true),
    DynamicWaypoint(-90.8, -253.4, 6.3, 0.4, 0, true),
    DynamicWaypoint(-64.3, -241.2, 9.2, 0.4, 0, true),
    DynamicWaypoint(-40.2, -232.4, 10.2, 0.3, 0, true),
    DynamicWaypoint(-20.9, -232.6, 10.1, 6.1, 0, true),
    DynamicWaypoint(-0.6, -241.6, 11.5, 5.9, 0, true),
    DynamicWaypoint(22.0, -243.8, 14.0, 6.2, 0, true),
    DynamicWaypoint(43.9, -249.8, 15.3, 6.0, 0, true),
    DynamicWaypoint(65.4, -261.4, 19.0, 5.8, 0, true),
    DynamicWaypoint(82.4, -273.7, 23.7, 5.6, 0, true),
    DynamicWaypoint(86.3, -288.4, 27.4, 5.0, 0, true),
    DynamicWaypoint(93.3, -304.4, 31.8, 5.2, 0, true),
    DynamicWaypoint(99.9, -318.1, 35.6, 5.2, 0, true),
    DynamicWaypoint(106.2, -331.2, 39.2, 5.2, 0, true),
    DynamicWaypoint(113.2, -345.4, 41.8, 5.2, 0, true),
    DynamicWaypoint(120.8, -360.5, 43.1, 5.2, 0, true),
    DynamicWaypoint(130.4, -379.9, 42.6, 5.2, 0, true),
    DynamicWaypoint(143.3, -391.4, 42.5, 5.7, 0, true),
    DynamicWaypoint(161.6, -400.5, 42.6, 5.9, 0, true),
    DynamicWaypoint(182.8, -406.7, 42.7, 6.1, 0, true),
    DynamicWaypoint(202.5, -412.3, 42.7, 6.0, 0, true),
    DynamicWaypoint(221.0, -417.7, 39.8, 6.0, 0, true),
    DynamicWaypoint(234.7, -420.3, 37.7, 0.0, 0, true),
    DynamicWaypoint(250.9, -412.0, 32.7, 0.5, 0, true),
    DynamicWaypoint(265.2, -401.1, 21.2, 0.8, 0, true),
    DynamicWaypoint(278.9, -389.4, 9.2, 0.6, 0, true),
    DynamicWaypoint(296.9, -382.7, 2.2, 0.2, 0, true),
    DynamicWaypoint(317.3, -381.4, -0.9, 0.0, 0, true),
    DynamicWaypoint(335.4, -386.0, -0.7, 6.0, 0, true),
    DynamicWaypoint(353.1, -389.3, -0.5, 6.1, 0, true),
    DynamicWaypoint(368.7, -391.8, -0.3, 6.1, 0, true),
    DynamicWaypoint(389.0, -393.7, -1.1, 0.0, 0, true),
    DynamicWaypoint(404.4, -388.2, -1.2, 0.3, 0, true),
    DynamicWaypoint(424.3, -381.2, -1.2, 0.3, 0, true),
    DynamicWaypoint(444.3, -374.9, -1.2, 0.3, 0, true),
    DynamicWaypoint(465.5, -368.3, -1.2, 0.3, 0, true),
    DynamicWaypoint(482.1, -355.5, -1.2, 0.7, 0, true),
    DynamicWaypoint(499.1, -341.2, -1.1, 0.7, 0, true),
    DynamicWaypoint(512.0, -330.2, -1.1, 0.7, 0, true),
    DynamicWaypoint(529.3, -321.8, 1.1, 0.1, 0, true),
    DynamicWaypoint(547.9, -320.8, 9.4, 6.3, 0, true),
    DynamicWaypoint(561.1, -325.6, 17.8, 5.9, 0, true),
    DynamicWaypoint(574.4, -329.8, 25.8, 6.0, 0, true),
    DynamicWaypoint(587.1, -334.3, 30.1, 5.9, 0, true),
    DynamicWaypoint(608.5, -335.3, 30.4, 0.2, 0, true),
    DynamicWaypoint(627.2, -319.4, 30.1, 0.7, 0, true),
    DynamicWaypoint(636.6, -297.5, 30.1, 1.3, 0, true),
    DynamicWaypoint(637.4, -269.5, 30.0, 1.6, 0, true),
    DynamicWaypoint(635.3, -248.6, 34.9, 1.7, 0, true),
    DynamicWaypoint(631.2, -227.4, 37.9, 1.8, 0, true),
    DynamicWaypoint(627.2, -205.0, 39.1, 1.7, 0, true),
    DynamicWaypoint(623.7, -180.7, 38.0, 1.7, 0, true),
    DynamicWaypoint(619.9, -153.6, 33.7, 1.7, 0, true),
    DynamicWaypoint(619.3, -130.9, 33.7, 1.5, 0, true),
    DynamicWaypoint(625.3, -111.4, 37.8, 1.2, 0, true),
    DynamicWaypoint(630.4, -94.6, 41.2, 1.3, 0, true),
    DynamicWaypoint(634.4, -74.0, 41.7, 1.4, 0, true),
    DynamicWaypoint(635.1, -51.3, 42.4, 1.6, 0, true),
    DynamicWaypoint(644.2, -38.0, 46.1, 0.7, 0, true),
    DynamicWaypoint(658.4, -30.2, 49.5, 0.3, 0, true),
    DynamicWaypoint(678.5, -24.0, 50.6, 0.3, 0, true),
    DynamicWaypoint(694.1, -19.1, 50.6, 0.3, 0, true),
    DynamicWaypoint(690.9, -3.1, 50.6, 1.5, 0, true),
    DynamicWaypoint(705.1, 0.5, 50.6, 6.1, 0, true),
    DynamicWaypoint(725.2, -8.9, 50.6, 5.9, 0, true)};

static DynamicWaypoint horde_field_path[] = {
    DynamicWaypoint(-311.0, -142.6, 12.5, 4.3, 0, true),
    DynamicWaypoint(-317.1, -161.6, 9.3, 4.4, 0, true),
    DynamicWaypoint(-313.1, -176.5, 9.3, 5.1, 0, true),
    DynamicWaypoint(-298.5, -191.6, 8.9, 5.5, 0, true),
    DynamicWaypoint(-285.5, -212.2, 8.3, 5.2, 0, true),
    DynamicWaypoint(-291.5, -233.4, 10.3, 4.4, 0, true),
    DynamicWaypoint(-299.6, -256.0, 6.9, 4.4, 0, true),
    DynamicWaypoint(-312.0, -271.5, 6.7, 4.0, 0, true),
    DynamicWaypoint(-325.5, -291.3, 6.7, 4.2, 0, true),
    DynamicWaypoint(-318.7, -315.4, 6.7, 5.0, 0, true),
    DynamicWaypoint(-310.0, -343.2, 6.7, 5.0, 0, true),
    DynamicWaypoint(-291.7, -355.8, 6.7, 6.2, 0, true),
    DynamicWaypoint(-266.1, -354.6, 6.7, 0.0, 0, true),
    DynamicWaypoint(-243.2, -346.2, 7.5, 0.5, 0, true),
    DynamicWaypoint(-220.0, -336.7, 8.8, 0.4, 0, true),
    DynamicWaypoint(-195.9, -330.0, 7.8, 0.2, 0, true),
    DynamicWaypoint(-174.6, -324.0, 8.7, 0.3, 0, true),
    DynamicWaypoint(-157.1, -308.6, 10.3, 0.7, 0, true),
    DynamicWaypoint(-142.0, -287.3, 7.6, 0.7, 0, true),
    DynamicWaypoint(-122.3, -274.7, 6.7, 0.6, 0, true),
    DynamicWaypoint(-102.3, -260.5, 6.5, 0.6, 0, true),
    DynamicWaypoint(-80.3, -248.6, 7.2, 0.5, 0, true),
    DynamicWaypoint(-58.3, -237.7, 9.6, 0.5, 0, true),
    DynamicWaypoint(-39.2, -232.4, 10.2, 0.1, 0, true),
    DynamicWaypoint(-19.7, -234.1, 10.1, 5.9, 0, true),
    DynamicWaypoint(-1.6, -240.6, 11.4, 5.9, 0, true),
    DynamicWaypoint(9.9, -242.7, 12.5, 6.2, 0, true),
    DynamicWaypoint(35.9, -246.4, 14.6, 6.1, 0, true),
    DynamicWaypoint(55.4, -254.0, 16.7, 5.8, 0, true),
    DynamicWaypoint(74.8, -265.8, 21.1, 5.7, 0, true),
    DynamicWaypoint(84.0, -277.9, 24.8, 5.3, 0, true),
    DynamicWaypoint(90.2, -296.7, 29.7, 5.1, 0, true),
    DynamicWaypoint(97.4, -312.7, 34.1, 5.1, 0, true),
    DynamicWaypoint(104.1, -326.3, 37.9, 5.2, 0, true),
    DynamicWaypoint(110.5, -338.8, 40.7, 5.2, 0, true),
    DynamicWaypoint(120.7, -359.2, 43.0, 5.2, 0, true),
    DynamicWaypoint(133.1, -383.0, 42.3, 5.2, 0, true),
    DynamicWaypoint(153.6, -396.9, 42.8, 5.8, 0, true),
    DynamicWaypoint(176.3, -404.3, 42.6, 6.0, 0, true),
    DynamicWaypoint(194.7, -409.7, 43.0, 6.0, 0, true),
    DynamicWaypoint(211.0, -414.6, 41.5, 6.0, 0, true),
    DynamicWaypoint(232.3, -420.5, 38.3, 6.1, 0, true),
    DynamicWaypoint(251.3, -413.0, 32.7, 0.4, 0, true),
    DynamicWaypoint(264.2, -403.0, 22.6, 0.7, 0, true),
    DynamicWaypoint(277.1, -392.0, 10.8, 0.7, 0, true),
    DynamicWaypoint(295.9, -383.2, 2.5, 0.3, 0, true),
    DynamicWaypoint(317.4, -381.3, -0.9, 0.0, 0, true),
    DynamicWaypoint(334.6, -383.9, -0.7, 6.1, 0, true),
    DynamicWaypoint(351.2, -387.2, -0.2, 6.1, 0, true),
    DynamicWaypoint(368.9, -391.3, -0.3, 6.1, 0, true),
    DynamicWaypoint(395.6, -390.7, -1.2, 0.2, 0, true),
    DynamicWaypoint(420.6, -381.2, -1.2, 0.4, 0, true),
    DynamicWaypoint(445.2, -375.9, -1.2, 0.2, 0, true),
    DynamicWaypoint(467.9, -365.3, -1.2, 0.4, 0, true),
    DynamicWaypoint(487.9, -346.6, -1.2, 0.7, 0, true),
    DynamicWaypoint(511.9, -329.0, -1.1, 0.6, 0, true),
    DynamicWaypoint(526.7, -322.4, 0.4, 0.2, 0, true),
    DynamicWaypoint(544.1, -321.5, 7.1, 6.3, 0, true),
    DynamicWaypoint(557.4, -323.3, 15.6, 6.1, 0, true),
    DynamicWaypoint(566.3, -326.4, 20.7, 5.9, 0, true),
    DynamicWaypoint(577.8, -330.6, 27.7, 5.9, 0, true),
    DynamicWaypoint(585.4, -333.5, 30.0, 5.9, 0, true),
    DynamicWaypoint(599.5, -337.1, 30.3, 6.2, 0, true),
    DynamicWaypoint(618.4, -328.4, 30.1, 0.8, 0, true),
    DynamicWaypoint(630.5, -312.0, 30.1, 1.1, 0, true),
    DynamicWaypoint(637.0, -292.1, 30.1, 1.3, 0, true),
    DynamicWaypoint(637.2, -269.5, 30.0, 1.7, 0, true),
    DynamicWaypoint(634.7, -250.9, 34.5, 1.7, 0, true),
    DynamicWaypoint(631.7, -231.9, 37.5, 1.7, 0, true),
    DynamicWaypoint(629.0, -214.0, 38.9, 1.7, 0, true),
    DynamicWaypoint(626.3, -196.1, 39.0, 1.7, 0, true),
    DynamicWaypoint(623.6, -178.8, 37.8, 1.7, 0, true),
    DynamicWaypoint(620.1, -153.9, 33.7, 1.7, 0, true),
    DynamicWaypoint(619.0, -137.1, 33.4, 1.5, 0, true),
    DynamicWaypoint(622.3, -121.7, 35.2, 1.3, 0, true),
    DynamicWaypoint(628.8, -104.8, 39.6, 1.2, 0, true),
    DynamicWaypoint(631.7, -89.3, 41.4, 1.4, 0, true),
    DynamicWaypoint(634.4, -70.9, 41.7, 1.5, 0, true),
    DynamicWaypoint(634.5, -50.5, 42.4, 1.6, 0, true),
    DynamicWaypoint(645.9, -37.4, 46.4, 0.7, 0, true),
    DynamicWaypoint(659.1, -30.1, 49.7, 0.4, 0, true),
    DynamicWaypoint(678.4, -23.3, 50.6, 0.3, 0, true),
    DynamicWaypoint(694.1, -18.7, 50.6, 0.4, 0, true),
    DynamicWaypoint(691.1, -5.0, 50.6, 1.7, 0, true),
    DynamicWaypoint(703.1, 1.4, 50.6, 0.2, 0, true),
    DynamicWaypoint(726.6, -10.1, 50.6, 5.9, 0, true)};

void alterac_valley::do_ground_assault(Team team, Creature* commander)
{
    float(*sf_grp)[4] = team == ALLIANCE ? ally_sf_grp : horde_sf_grp;
    float(*field_grp)[4] = team == ALLIANCE ? ally_field_grp : horde_field_grp;
    ground_data* data = team == ALLIANCE ? &ground_data_[0] : &ground_data_[1];
    data->reset(this);

    uint32 upgrade_lvl =
        GetData(team == ALLIANCE ? ALLIANCE_SOLDIERS_UPGRADE_LEVEL :
                                   HORDE_SOLDIERS_UPGRADE_LEVEL);
    // alliance entries: 13524, 13525, 13526, 13527
    // horde entries: 13528, 13529, 13530, 13531
    uint32 soldier_entry = 13524 + (team == ALLIANCE ? 0 : 4) + upgrade_lvl;

    data->grp_id[0] = instance->GetCreatureGroupMgr().CreateNewGroup(
        team == ALLIANCE ? "assault ally #1" : "assault horde #2", true);
    data->grp_id[1] = instance->GetCreatureGroupMgr().CreateNewGroup(
        team == ALLIANCE ? "assault ally #2" : "assault horde #2", true);
    auto grpone = instance->GetCreatureGroupMgr().GetGroup(data->grp_id[0]);
    auto grptwo = instance->GetCreatureGroupMgr().GetGroup(data->grp_id[1]);
    if (!grpone || !grptwo)
        return;

    grptwo->AddMember(commander, false);

    int units = 4 + (int)upgrade_lvl * 2; // 4, 6, 8, 10
    // Snowfall group
    for (int i = 0; i < units; ++i)
    {
        if (Creature* c = commander->SummonCreature(soldier_entry, sf_grp[i][0],
                sf_grp[i][1], sf_grp[i][2], sf_grp[i][3],
                TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10 * IN_MILLISECONDS,
                SUMMON_OPT_ACTIVE))
        {
            c->movement_gens.remove_all(movement::gen::idle);
            grpone->AddMember(c, false);
            data->grp_one.push_back(c->GetObjectGuid());
        }
    }
    // Fields of strife group
    for (int i = 0; i < units; ++i)
    {
        if (Creature* c = commander->SummonCreature(soldier_entry,
                field_grp[i][0], field_grp[i][1], field_grp[i][2],
                field_grp[i][3], TEMPSUMMON_CORPSE_TIMED_DESPAWN,
                10 * IN_MILLISECONDS, SUMMON_OPT_ACTIVE))
        {
            c->movement_gens.remove_all(movement::gen::idle);
            grptwo->AddMember(c, false);
            data->grp_two.push_back(c->GetObjectGuid());
        }
    }

    // Walk commander to front of fields of strife group
    std::vector<DynamicWaypoint> wps;
    wps.push_back(
        team == ALLIANCE ? ally_commander_start : horde_commander_start);
    commander->movement_gens.remove_all(movement::gen::idle);
    commander->movement_gens.push(
        new movement::DynamicWaypointMovementGenerator(wps, false));

    commander->RemoveFlag(
        UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);

    data->stage = 0;
    data->timer = 20000;
    data->commander = commander->GetObjectGuid();
}

void alterac_valley::update_ground(Team team)
{
    ground_data* data = team == ALLIANCE ? &ground_data_[0] : &ground_data_[1];
    Creature* commander = instance->GetCreature(data->commander);
    if (!commander)
        return; // XXX: cancel event

    if (data->stage == 0 && data->timer == 0)
    {
        data->stage = 1;

        auto grpone = instance->GetCreatureGroupMgr().GetGroup(data->grp_id[0]);
        auto grptwo = instance->GetCreatureGroupMgr().GetGroup(data->grp_id[1]);
        if (!grpone || !grptwo)
            return; // XXX: cancel event

        commander->MonsterSay("Onwards!", 0); // XXX: made-up say

        DynamicWaypoint* one_begin = team == ALLIANCE ?
                                         std::begin(ally_sf_path) :
                                         std::begin(horde_sf_path);
        DynamicWaypoint* one_end =
            team == ALLIANCE ? std::end(ally_sf_path) : std::end(horde_sf_path);
        DynamicWaypoint* two_begin = team == ALLIANCE ?
                                         std::begin(ally_field_path) :
                                         std::begin(horde_field_path);
        DynamicWaypoint* two_end = team == ALLIANCE ?
                                       std::end(ally_field_path) :
                                       std::end(horde_field_path);

        // Start movement for the Snowfall Group
        {
            grpone->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);
            for (auto itr = one_begin; itr != one_end; ++itr)
                instance->GetCreatureGroupMgr().GetMovementMgr().AddWaypoint(
                    data->grp_id[0], *itr);
            for_each_creature(data->grp_one, [this, data](Creature* c)
                {
                    instance->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .SetNewFormation(data->grp_id[0], c);
                });
            instance->GetCreatureGroupMgr().GetMovementMgr().StartMovement(
                data->grp_id[0], grpone->GetMembers());
        }

        // Start movement for the Fields of Strife group
        {
            grptwo->AddFlag(CREATURE_GROUP_FLAG_GROUP_MOVEMENT);
            for (auto itr = two_begin; itr != two_end; ++itr)
                instance->GetCreatureGroupMgr().GetMovementMgr().AddWaypoint(
                    data->grp_id[1], *itr);
            for_each_creature(data->grp_two, [this, data](Creature* c)
                {
                    instance->GetCreatureGroupMgr()
                        .GetMovementMgr()
                        .SetNewFormation(data->grp_id[1], c);
                });
            instance->GetCreatureGroupMgr().GetMovementMgr().SetNewFormation(
                data->grp_id[1], commander);
            instance->GetCreatureGroupMgr().GetMovementMgr().StartMovement(
                data->grp_id[1], grptwo->GetMembers());
        }
    }
}

void alterac_valley::ground_data::reset(alterac_valley* av)
{
    timer = 0;
    stage = 0;
    commander.Clear();
    grp_one.clear();
    grp_two.clear();
    if (auto grp = av->instance->GetCreatureGroupMgr().GetGroup(grp_id[0]))
    {
        if (grp->IsTemporaryGroup())
            av->instance->GetCreatureGroupMgr().DeleteGroup(grp_id[0]);
    }
    if (auto grp = av->instance->GetCreatureGroupMgr().GetGroup(grp_id[1]))
    {
        if (grp->IsTemporaryGroup())
            av->instance->GetCreatureGroupMgr().DeleteGroup(grp_id[1]);
    }
    grp_id[0] = 0;
    grp_id[1] = 0;
}
