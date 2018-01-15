-- Add a next_spell column
ALTER TABLE spell_chain ADD next_spell VARCHAR(127) AFTER prev_spell;

-- Add next to all existing entries
UPDATE spell_chain SET next_spell='10473' WHERE spell_id = 10472;
UPDATE spell_chain SET next_spell='20909' WHERE spell_id = 19306;
UPDATE spell_chain SET next_spell='27841' WHERE spell_id = 14819;
UPDATE spell_chain SET next_spell='25442' WHERE spell_id = 25439;
UPDATE spell_chain SET next_spell='6075' WHERE spell_id = 6074;
UPDATE spell_chain SET next_spell='27210' WHERE spell_id = 17923;
UPDATE spell_chain SET next_spell='13549' WHERE spell_id = 1978;
UPDATE spell_chain SET next_spell='27014' WHERE spell_id = 14266;
UPDATE spell_chain SET next_spell='10431' WHERE spell_id = 8134;
UPDATE spell_chain SET next_spell='20748' WHERE spell_id = 20747;
UPDATE spell_chain SET next_spell='10138' WHERE spell_id = 6127;
UPDATE spell_chain SET next_spell='7648' WHERE spell_id = 6223;
UPDATE spell_chain SET next_spell='11400' WHERE spell_id = 8694;
UPDATE spell_chain SET next_spell='10456' WHERE spell_id = 8038;
UPDATE spell_chain SET next_spell='13228' WHERE spell_id = 13220;
UPDATE spell_chain SET next_spell='15431' WHERE spell_id = 15430;
UPDATE spell_chain SET next_spell='8438' WHERE spell_id = 8437;
UPDATE spell_chain SET next_spell='10901' WHERE spell_id = 10900;
UPDATE spell_chain SET next_spell='9841' WHERE spell_id = 9840;
UPDATE spell_chain SET next_spell='598' WHERE spell_id = 591;
UPDATE spell_chain SET next_spell='25525' WHERE spell_id = 10428;
UPDATE spell_chain SET next_spell='19271' WHERE spell_id = 13896;
UPDATE spell_chain SET next_spell='14260' WHERE spell_id = 2973;
UPDATE spell_chain SET next_spell='10406' WHERE spell_id = 8155;
UPDATE spell_chain SET next_spell='25368' WHERE spell_id = 25367;
UPDATE spell_chain SET next_spell='26984' WHERE spell_id = 9912;
UPDATE spell_chain SET next_spell='8462' WHERE spell_id = 8461;
UPDATE spell_chain SET next_spell='6391' WHERE spell_id = 6390;
UPDATE spell_chain SET next_spell='25290' WHERE spell_id = 19854;
UPDATE spell_chain SET next_spell='10623' WHERE spell_id = 10622;
UPDATE spell_chain SET next_spell='9901' WHERE spell_id = 8955;
UPDATE spell_chain SET next_spell='547' WHERE spell_id = 332;
UPDATE spell_chain SET next_spell='44206' WHERE spell_id = 44205;
UPDATE spell_chain SET next_spell='27250' WHERE spell_id = 17953;
UPDATE spell_chain SET next_spell='38697' WHERE spell_id = 27072;
UPDATE spell_chain SET next_spell='34863' WHERE spell_id = 34861;
UPDATE spell_chain SET next_spell='20290' WHERE spell_id = 20289;
UPDATE spell_chain SET next_spell='16353' WHERE spell_id = 16352;
UPDATE spell_chain SET next_spell='19243' WHERE spell_id = 19242;
UPDATE spell_chain SET next_spell='11354' WHERE spell_id = 11353;
UPDATE spell_chain SET next_spell='20912' WHERE spell_id = 20911;
UPDATE spell_chain SET next_spell='984' WHERE spell_id = 598;
UPDATE spell_chain SET next_spell='8134' WHERE spell_id = 945;
UPDATE spell_chain SET next_spell='17951' WHERE spell_id = 6366;
UPDATE spell_chain SET next_spell='27173' WHERE spell_id = 20924;
UPDATE spell_chain SET next_spell='1430' WHERE spell_id = 1058;
UPDATE spell_chain SET next_spell='14324' WHERE spell_id = 14323;
UPDATE spell_chain SET next_spell='27020' WHERE spell_id = 15632;
UPDATE spell_chain SET next_spell='1062' WHERE spell_id = 339;
UPDATE spell_chain SET next_spell='13552' WHERE spell_id = 13551;
UPDATE spell_chain SET next_spell='9758' WHERE spell_id = 8903;
UPDATE spell_chain SET next_spell='9839' WHERE spell_id = 8910;
UPDATE spell_chain SET next_spell='20752' WHERE spell_id = 693;
UPDATE spell_chain SET next_spell='25457' WHERE spell_id = 29228;
UPDATE spell_chain SET next_spell='27220' WHERE spell_id = 27219;
UPDATE spell_chain SET next_spell='2052' WHERE spell_id = 2050;
UPDATE spell_chain SET next_spell='27136' WHERE spell_id = 27135;
UPDATE spell_chain SET next_spell='7301' WHERE spell_id = 7300;
UPDATE spell_chain SET next_spell='5179' WHERE spell_id = 5178;
UPDATE spell_chain SET next_spell='14273' WHERE spell_id = 14272;
UPDATE spell_chain SET next_spell='945' WHERE spell_id = 905;
UPDATE spell_chain SET next_spell='25235' WHERE spell_id = 25233;
UPDATE spell_chain SET next_spell='3747' WHERE spell_id = 600;
UPDATE spell_chain SET next_spell='2121' WHERE spell_id = 2120;
UPDATE spell_chain SET next_spell='9756' WHERE spell_id = 8914;
UPDATE spell_chain SET next_spell='24974' WHERE spell_id = 5570;
UPDATE spell_chain SET next_spell='19834' WHERE spell_id = 19740;
UPDATE spell_chain SET next_spell='25359' WHERE spell_id = 10627;
UPDATE spell_chain SET next_spell='8289' WHERE spell_id = 8288;
UPDATE spell_chain SET next_spell='27134' WHERE spell_id = 13033;
UPDATE spell_chain SET next_spell='11704' WHERE spell_id = 11703;
UPDATE spell_chain SET next_spell='915' WHERE spell_id = 548;
UPDATE spell_chain SET next_spell='10468' WHERE spell_id = 10467;
UPDATE spell_chain SET next_spell='10893' WHERE spell_id = 10892;
UPDATE spell_chain SET next_spell='11341' WHERE spell_id = 8691;
UPDATE spell_chain SET next_spell='30911' WHERE spell_id = 27264;
UPDATE spell_chain SET next_spell='913' WHERE spell_id = 547;
UPDATE spell_chain SET next_spell='8027' WHERE spell_id = 8024;
UPDATE spell_chain SET next_spell='27800' WHERE spell_id = 27799;
UPDATE spell_chain SET next_spell='5187' WHERE spell_id = 5186;
UPDATE spell_chain SET next_spell='8161' WHERE spell_id = 8160;
UPDATE spell_chain SET next_spell='14286' WHERE spell_id = 14285;
UPDATE spell_chain SET next_spell='20924' WHERE spell_id = 20923;
UPDATE spell_chain SET next_spell='17925' WHERE spell_id = 6789;
UPDATE spell_chain SET next_spell='5180' WHERE spell_id = 5179;
UPDATE spell_chain SET next_spell='1245' WHERE spell_id = 1244;
UPDATE spell_chain SET next_spell='27072' WHERE spell_id = 27071;
UPDATE spell_chain SET next_spell='20162' WHERE spell_id = 21082;
UPDATE spell_chain SET next_spell='10952' WHERE spell_id = 10951;
UPDATE spell_chain SET next_spell='33944' WHERE spell_id = 10174;
UPDATE spell_chain SET next_spell='22783' WHERE spell_id = 22782;
UPDATE spell_chain SET next_spell='8012' WHERE spell_id = 370;
UPDATE spell_chain SET next_spell='25567' WHERE spell_id = 10463;
UPDATE spell_chain SET next_spell='6363' WHERE spell_id = 3599;
UPDATE spell_chain SET next_spell='27130' WHERE spell_id = 10170;
UPDATE spell_chain SET next_spell='20777' WHERE spell_id = 20776;
UPDATE spell_chain SET next_spell='10206' WHERE spell_id = 10205;
UPDATE spell_chain SET next_spell='10961' WHERE spell_id = 10960;
UPDATE spell_chain SET next_spell='10157' WHERE spell_id = 10156;
UPDATE spell_chain SET next_spell='9578' WHERE spell_id = 586;
UPDATE spell_chain SET next_spell='25345' WHERE spell_id = 10212;
UPDATE spell_chain SET next_spell='10953' WHERE spell_id = 8192;
UPDATE spell_chain SET next_spell='8019' WHERE spell_id = 8018;
UPDATE spell_chain SET next_spell='10945' WHERE spell_id = 8106;
UPDATE spell_chain SET next_spell='10912' WHERE spell_id = 10911;
UPDATE spell_chain SET next_spell='10187' WHERE spell_id = 10186;
UPDATE spell_chain SET next_spell='6223' WHERE spell_id = 6222;
UPDATE spell_chain SET next_spell='27221' WHERE spell_id = 11704;
UPDATE spell_chain SET next_spell='1108' WHERE spell_id = 702;
UPDATE spell_chain SET next_spell='19240' WHERE spell_id = 19238;
UPDATE spell_chain SET next_spell='27070' WHERE spell_id = 25306;
UPDATE spell_chain SET next_spell='9888' WHERE spell_id = 9758;
UPDATE spell_chain SET next_spell='699' WHERE spell_id = 689;
UPDATE spell_chain SET next_spell='14202' WHERE spell_id = 14201;
UPDATE spell_chain SET next_spell='20757' WHERE spell_id = 20756;
UPDATE spell_chain SET next_spell='19282' WHERE spell_id = 19281;
UPDATE spell_chain SET next_spell='10951' WHERE spell_id = 1006;
UPDATE spell_chain SET next_spell='26978' WHERE spell_id = 25297;
UPDATE spell_chain SET next_spell='30909' WHERE spell_id = 27224;
UPDATE spell_chain SET next_spell='5627' WHERE spell_id = 2878;
UPDATE spell_chain SET next_spell='10145' WHERE spell_id = 10144;
UPDATE spell_chain SET next_spell='20190' WHERE spell_id = 20043;
UPDATE spell_chain SET next_spell='10324' WHERE spell_id = 10322;
UPDATE spell_chain SET next_spell='8498' WHERE spell_id = 1535;
UPDATE spell_chain SET next_spell='25464' WHERE spell_id = 10473;
UPDATE spell_chain SET next_spell='10875' WHERE spell_id = 10874;
UPDATE spell_chain SET next_spell='10180' WHERE spell_id = 10179;
UPDATE spell_chain SET next_spell='25501' WHERE spell_id = 16353;
UPDATE spell_chain SET next_spell='19274' WHERE spell_id = 19273;
UPDATE spell_chain SET next_spell='11703' WHERE spell_id = 6226;
UPDATE spell_chain SET next_spell='10151' WHERE spell_id = 10150;
UPDATE spell_chain SET next_spell='10495' WHERE spell_id = 5675;
UPDATE spell_chain SET next_spell='18869' WHERE spell_id = 18868;
UPDATE spell_chain SET next_spell='8439' WHERE spell_id = 8438;
UPDATE spell_chain SET next_spell='10442' WHERE spell_id = 8161;
UPDATE spell_chain SET next_spell='10911' WHERE spell_id = 605;
UPDATE spell_chain SET next_spell='32999' WHERE spell_id = 27681;
UPDATE spell_chain SET next_spell='19266' WHERE spell_id = 19265;
UPDATE spell_chain SET next_spell='30404' WHERE spell_id = 30108;
UPDATE spell_chain SET next_spell='8951' WHERE spell_id = 8950;
UPDATE spell_chain SET next_spell='30413' WHERE spell_id = 30283;
UPDATE spell_chain SET next_spell='27230' WHERE spell_id = 11730;
UPDATE spell_chain SET next_spell='25467' WHERE spell_id = 19280;
UPDATE spell_chain SET next_spell='8495' WHERE spell_id = 8494;
UPDATE spell_chain SET next_spell='27179' WHERE spell_id = 20928;
UPDATE spell_chain SET next_spell='18937' WHERE spell_id = 18220;
UPDATE spell_chain SET next_spell='21850' WHERE spell_id = 21849;
UPDATE spell_chain SET next_spell='33933' WHERE spell_id = 27133;
UPDATE spell_chain SET next_spell='10460' WHERE spell_id = 6372;
UPDATE spell_chain SET next_spell='5196' WHERE spell_id = 5195;
UPDATE spell_chain SET next_spell='10318' WHERE spell_id = 2812;
UPDATE spell_chain SET next_spell='6131' WHERE spell_id = 865;
UPDATE spell_chain SET next_spell='32796' WHERE spell_id = 28609;
UPDATE spell_chain SET next_spell='15208' WHERE spell_id = 15207;
UPDATE spell_chain SET next_spell='27088' WHERE spell_id = 10230;
UPDATE spell_chain SET next_spell='14287' WHERE spell_id = 14286;
UPDATE spell_chain SET next_spell='20348' WHERE spell_id = 20347;
UPDATE spell_chain SET next_spell='19837' WHERE spell_id = 19836;
UPDATE spell_chain SET next_spell='19308' WHERE spell_id = 18137;
UPDATE spell_chain SET next_spell='10934' WHERE spell_id = 10933;
UPDATE spell_chain SET next_spell='18880' WHERE spell_id = 18879;
UPDATE spell_chain SET next_spell='30405' WHERE spell_id = 30404;
UPDATE spell_chain SET next_spell='11739' WHERE spell_id = 6229;
UPDATE spell_chain SET next_spell='10437' WHERE spell_id = 6365;
UPDATE spell_chain SET next_spell='6129' WHERE spell_id = 990;
UPDATE spell_chain SET next_spell='27215' WHERE spell_id = 25309;
UPDATE spell_chain SET next_spell='8154' WHERE spell_id = 8071;
UPDATE spell_chain SET next_spell='9876' WHERE spell_id = 9875;
UPDATE spell_chain SET next_spell='10140' WHERE spell_id = 10139;
UPDATE spell_chain SET next_spell='8941' WHERE spell_id = 8940;
UPDATE spell_chain SET next_spell='17921' WHERE spell_id = 17920;
UPDATE spell_chain SET next_spell='27078' WHERE spell_id = 10199;
UPDATE spell_chain SET next_spell='15631' WHERE spell_id = 15630;
UPDATE spell_chain SET next_spell='14264' WHERE spell_id = 14263;
UPDATE spell_chain SET next_spell='30908' WHERE spell_id = 27221;
UPDATE spell_chain SET next_spell='10197' WHERE spell_id = 8413;
UPDATE spell_chain SET next_spell='32231' WHERE spell_id = 29722;
UPDATE spell_chain SET next_spell='11668' WHERE spell_id = 11667;
UPDATE spell_chain SET next_spell='20920' WHERE spell_id = 20919;
UPDATE spell_chain SET next_spell='27024' WHERE spell_id = 14301;
UPDATE spell_chain SET next_spell='27139' WHERE spell_id = 10318;
UPDATE spell_chain SET next_spell='25570' WHERE spell_id = 10497;
UPDATE spell_chain SET next_spell='25364' WHERE spell_id = 25363;
UPDATE spell_chain SET next_spell='3472' WHERE spell_id = 1042;
UPDATE spell_chain SET next_spell='5195' WHERE spell_id = 1062;
UPDATE spell_chain SET next_spell='18809' WHERE spell_id = 12526;
UPDATE spell_chain SET next_spell='6390' WHERE spell_id = 5730;
UPDATE spell_chain SET next_spell='8938' WHERE spell_id = 8936;
UPDATE spell_chain SET next_spell='1244' WHERE spell_id = 1243;
UPDATE spell_chain SET next_spell='18867' WHERE spell_id = 17877;
UPDATE spell_chain SET next_spell='9907' WHERE spell_id = 9749;
UPDATE spell_chain SET next_spell='32593' WHERE spell_id = 974;
UPDATE spell_chain SET next_spell='9863' WHERE spell_id = 9862;
UPDATE spell_chain SET next_spell='20349' WHERE spell_id = 20348;
UPDATE spell_chain SET next_spell='8903' WHERE spell_id = 6778;
UPDATE spell_chain SET next_spell='17039 17040 17041' WHERE spell_id = 9787;
UPDATE spell_chain SET next_spell='19943' WHERE spell_id = 19942;
UPDATE spell_chain SET next_spell='11689' WHERE spell_id = 11688;
UPDATE spell_chain SET next_spell='13553' WHERE spell_id = 13552;
UPDATE spell_chain SET next_spell='8407' WHERE spell_id = 8406;
UPDATE spell_chain SET next_spell='27071' WHERE spell_id = 25304;
UPDATE spell_chain SET next_spell='8235' WHERE spell_id = 8232;
UPDATE spell_chain SET next_spell='28189' WHERE spell_id = 28176;
UPDATE spell_chain SET next_spell='28275' WHERE spell_id = 27871;
UPDATE spell_chain SET next_spell='42234' WHERE spell_id = 42245;
UPDATE spell_chain SET next_spell='14302' WHERE spell_id = 13795;
UPDATE spell_chain SET next_spell='25292' WHERE spell_id = 10329;
UPDATE spell_chain SET next_spell='32594' WHERE spell_id = 32593;
UPDATE spell_chain SET next_spell='6127' WHERE spell_id = 5506;
UPDATE spell_chain SET next_spell='8422' WHERE spell_id = 2121;
UPDATE spell_chain SET next_spell='20306' WHERE spell_id = 20305;
UPDATE spell_chain SET next_spell='996' WHERE spell_id = 596;
UPDATE spell_chain SET next_spell='10478' WHERE spell_id = 8181;
UPDATE spell_chain SET next_spell='27871' WHERE spell_id = 27870;
UPDATE spell_chain SET next_spell='25315' WHERE spell_id = 10929;
UPDATE spell_chain SET next_spell='25375' WHERE spell_id = 25372;
UPDATE spell_chain SET next_spell='11726' WHERE spell_id = 11725;
UPDATE spell_chain SET next_spell='26990' WHERE spell_id = 9885;
UPDATE spell_chain SET next_spell='27148' WHERE spell_id = 27147;
UPDATE spell_chain SET next_spell='15265' WHERE spell_id = 15264;
UPDATE spell_chain SET next_spell='6141' WHERE spell_id = 10;
UPDATE spell_chain SET next_spell='26892' WHERE spell_id = 11343;
UPDATE spell_chain SET next_spell='16356' WHERE spell_id = 16355;
UPDATE spell_chain SET next_spell='332' WHERE spell_id = 331;
UPDATE spell_chain SET next_spell='17937' WHERE spell_id = 17862;
UPDATE spell_chain SET next_spell='14319' WHERE spell_id = 14318;
UPDATE spell_chain SET next_spell='10892' WHERE spell_id = 2767;
UPDATE spell_chain SET next_spell='837' WHERE spell_id = 205;
UPDATE spell_chain SET next_spell='27801' WHERE spell_id = 27800;
UPDATE spell_chain SET next_spell='25309' WHERE spell_id = 11668;
UPDATE spell_chain SET next_spell='27075' WHERE spell_id = 25345;
UPDATE spell_chain SET next_spell='10917' WHERE spell_id = 10916;
UPDATE spell_chain SET next_spell='27065' WHERE spell_id = 20904;
UPDATE spell_chain SET next_spell='10144' WHERE spell_id = 6129;
UPDATE spell_chain SET next_spell='8499' WHERE spell_id = 8498;
UPDATE spell_chain SET next_spell='6213' WHERE spell_id = 5782;
UPDATE spell_chain SET next_spell='1020' WHERE spell_id = 642;
UPDATE spell_chain SET next_spell='25372' WHERE spell_id = 10947;
UPDATE spell_chain SET next_spell='5506' WHERE spell_id = 5505;
UPDATE spell_chain SET next_spell='10312' WHERE spell_id = 5615;
UPDATE spell_chain SET next_spell='25505' WHERE spell_id = 16362;
UPDATE spell_chain SET next_spell='14280' WHERE spell_id = 14279;
UPDATE spell_chain SET next_spell='2791' WHERE spell_id = 1245;
UPDATE spell_chain SET next_spell='10159' WHERE spell_id = 8492;
UPDATE spell_chain SET next_spell='9857' WHERE spell_id = 9856;
UPDATE spell_chain SET next_spell='30545' WHERE spell_id = 27211;
UPDATE spell_chain SET next_spell='27228' WHERE spell_id = 11722;
UPDATE spell_chain SET next_spell='25391' WHERE spell_id = 25357;
UPDATE spell_chain SET next_spell='8038' WHERE spell_id = 8033;
UPDATE spell_chain SET next_spell='19312' WHERE spell_id = 19311;
UPDATE spell_chain SET next_spell='17728' WHERE spell_id = 17727;
UPDATE spell_chain SET next_spell='14314' WHERE spell_id = 13812;
UPDATE spell_chain SET next_spell='10223' WHERE spell_id = 8458;
UPDATE spell_chain SET next_spell='19238' WHERE spell_id = 19236;
UPDATE spell_chain SET next_spell='20755' WHERE spell_id = 20752;
UPDATE spell_chain SET next_spell='27015' WHERE spell_id = 14273;
UPDATE spell_chain SET next_spell='10230' WHERE spell_id = 6131;
UPDATE spell_chain SET next_spell='18932' WHERE spell_id = 18931;
UPDATE spell_chain SET next_spell='10139' WHERE spell_id = 10138;
UPDATE spell_chain SET next_spell='18658' WHERE spell_id = 18657;
UPDATE spell_chain SET next_spell='9592' WHERE spell_id = 9579;
UPDATE spell_chain SET next_spell='21564' WHERE spell_id = 21562;
UPDATE spell_chain SET next_spell='19284' WHERE spell_id = 19283;
UPDATE spell_chain SET next_spell='529' WHERE spell_id = 403;
UPDATE spell_chain SET next_spell='25304' WHERE spell_id = 10181;
UPDATE spell_chain SET next_spell='13544' WHERE spell_id = 13543;
UPDATE spell_chain SET next_spell='12824' WHERE spell_id = 118;
UPDATE spell_chain SET next_spell='25546' WHERE spell_id = 11315;
UPDATE spell_chain SET next_spell='19979' WHERE spell_id = 19978;
UPDATE spell_chain SET next_spell='25389' WHERE spell_id = 10938;
UPDATE spell_chain SET next_spell='12523' WHERE spell_id = 12522;
UPDATE spell_chain SET next_spell='27137' WHERE spell_id = 19943;
UPDATE spell_chain SET next_spell='27087' WHERE spell_id = 10161;
UPDATE spell_chain SET next_spell='12825' WHERE spell_id = 12824;
UPDATE spell_chain SET next_spell='27218' WHERE spell_id = 11713;
UPDATE spell_chain SET next_spell='10938' WHERE spell_id = 10937;
UPDATE spell_chain SET next_spell='27160' WHERE spell_id = 20349;
UPDATE spell_chain SET next_spell='8926' WHERE spell_id = 8925;
UPDATE spell_chain SET next_spell='25222' WHERE spell_id = 25221;
UPDATE spell_chain SET next_spell='10438' WHERE spell_id = 10437;
UPDATE spell_chain SET next_spell='26982' WHERE spell_id = 26981;
UPDATE spell_chain SET next_spell='7646' WHERE spell_id = 6205;
UPDATE spell_chain SET next_spell='11699' WHERE spell_id = 7651;
UPDATE spell_chain SET next_spell='25587' WHERE spell_id = 25585;
UPDATE spell_chain SET next_spell='10207' WHERE spell_id = 10206;
UPDATE spell_chain SET next_spell='10407' WHERE spell_id = 10406;
UPDATE spell_chain SET next_spell='19835' WHERE spell_id = 19834;
UPDATE spell_chain SET next_spell='25291' WHERE spell_id = 19838;
UPDATE spell_chain SET next_spell='25296' WHERE spell_id = 14322;
UPDATE spell_chain SET next_spell='10428' WHERE spell_id = 10427;
UPDATE spell_chain SET next_spell='548' WHERE spell_id = 529;
UPDATE spell_chain SET next_spell='19310' WHERE spell_id = 19309;
UPDATE spell_chain SET next_spell='7320' WHERE spell_id = 7302;
UPDATE spell_chain SET next_spell='20307' WHERE spell_id = 20306;
UPDATE spell_chain SET next_spell='13230' WHERE spell_id = 13229;
UPDATE spell_chain SET next_spell='33043' WHERE spell_id = 33042;
UPDATE spell_chain SET next_spell='19852' WHERE spell_id = 19850;
UPDATE spell_chain SET next_spell='11675' WHERE spell_id = 8289;
UPDATE spell_chain SET next_spell='17314' WHERE spell_id = 17313;
UPDATE spell_chain SET next_spell='10963' WHERE spell_id = 2060;
UPDATE spell_chain SET next_spell='20756' WHERE spell_id = 20755;
UPDATE spell_chain SET next_spell='25552' WHERE spell_id = 10587;
UPDATE spell_chain SET next_spell='19236' WHERE spell_id = 13908;
UPDATE spell_chain SET next_spell='9835' WHERE spell_id = 9834;
UPDATE spell_chain SET next_spell='8687' WHERE spell_id = 8681;
UPDATE spell_chain SET next_spell='34866' WHERE spell_id = 34865;
UPDATE spell_chain SET next_spell='19304' WHERE spell_id = 19303;
UPDATE spell_chain SET next_spell='10219' WHERE spell_id = 7320;
UPDATE spell_chain SET next_spell='14268' WHERE spell_id = 14267;
UPDATE spell_chain SET next_spell='20772' WHERE spell_id = 10324;
UPDATE spell_chain SET next_spell='26988' WHERE spell_id = 26987;
UPDATE spell_chain SET next_spell='5145' WHERE spell_id = 5144;
UPDATE spell_chain SET next_spell='20930' WHERE spell_id = 20929;
UPDATE spell_chain SET next_spell='25429' WHERE spell_id = 10942;
UPDATE spell_chain SET next_spell='27140' WHERE spell_id = 25291;
UPDATE spell_chain SET next_spell='6365' WHERE spell_id = 6364;
UPDATE spell_chain SET next_spell='6205' WHERE spell_id = 1108;
UPDATE spell_chain SET next_spell='25357' WHERE spell_id = 10396;
UPDATE spell_chain SET next_spell='11717' WHERE spell_id = 7659;
UPDATE spell_chain SET next_spell='8427' WHERE spell_id = 6141;
UPDATE spell_chain SET next_spell='12526' WHERE spell_id = 12525;
UPDATE spell_chain SET next_spell='27238' WHERE spell_id = 20757;
UPDATE spell_chain SET next_spell='10605' WHERE spell_id = 2860;
UPDATE spell_chain SET next_spell='14204' WHERE spell_id = 14203;
UPDATE spell_chain SET next_spell='10586' WHERE spell_id = 10585;
UPDATE spell_chain SET next_spell='8131' WHERE spell_id = 8129;
UPDATE spell_chain SET next_spell='14283' WHERE spell_id = 14282;
UPDATE spell_chain SET next_spell='8030' WHERE spell_id = 8027;
UPDATE spell_chain SET next_spell='10179' WHERE spell_id = 8408;
UPDATE spell_chain SET next_spell='10170' WHERE spell_id = 10169;
UPDATE spell_chain SET next_spell='592' WHERE spell_id = 17;
UPDATE spell_chain SET next_spell='8927' WHERE spell_id = 8926;
UPDATE spell_chain SET next_spell='20287' WHERE spell_id = 21084;
UPDATE spell_chain SET next_spell='145' WHERE spell_id = 143;
UPDATE spell_chain SET next_spell='19273' WHERE spell_id = 19271;
UPDATE spell_chain SET next_spell='6078' WHERE spell_id = 6077;
UPDATE spell_chain SET next_spell='19302' WHERE spell_id = 19299;
UPDATE spell_chain SET next_spell='25433' WHERE spell_id = 10958;
UPDATE spell_chain SET next_spell='19853' WHERE spell_id = 19852;
UPDATE spell_chain SET next_spell='19309' WHERE spell_id = 19308;
UPDATE spell_chain SET next_spell='20288' WHERE spell_id = 20287;
UPDATE spell_chain SET next_spell='15266' WHERE spell_id = 15265;
UPDATE spell_chain SET next_spell='17952' WHERE spell_id = 17951;
UPDATE spell_chain SET next_spell='11315' WHERE spell_id = 11314;
UPDATE spell_chain SET next_spell='27166' WHERE spell_id = 20357;
UPDATE spell_chain SET next_spell='10185' WHERE spell_id = 8427;
UPDATE spell_chain SET next_spell='33042' WHERE spell_id = 33041;
UPDATE spell_chain SET next_spell='10216' WHERE spell_id = 10215;
UPDATE spell_chain SET next_spell='10600' WHERE spell_id = 10595;
UPDATE spell_chain SET next_spell='12524' WHERE spell_id = 12523;
UPDATE spell_chain SET next_spell='970' WHERE spell_id = 594;
UPDATE spell_chain SET next_spell='10899' WHERE spell_id = 10898;
UPDATE spell_chain SET next_spell='8155' WHERE spell_id = 8154;
UPDATE spell_chain SET next_spell='8400' WHERE spell_id = 3140;
UPDATE spell_chain SET next_spell='17401' WHERE spell_id = 16914;
UPDATE spell_chain SET next_spell='20347' WHERE spell_id = 20165;
UPDATE spell_chain SET next_spell='9579' WHERE spell_id = 9578;
UPDATE spell_chain SET next_spell='17953' WHERE spell_id = 17952;
UPDATE spell_chain SET next_spell='20219 20222' WHERE spell_id = 12656;
UPDATE spell_chain SET next_spell='19305' WHERE spell_id = 19304;
UPDATE spell_chain SET next_spell='2137' WHERE spell_id = 2136;
UPDATE spell_chain SET next_spell='27223' WHERE spell_id = 17926;
UPDATE spell_chain SET next_spell='14267' WHERE spell_id = 2974;
UPDATE spell_chain SET next_spell='27044' WHERE spell_id = 25296;
UPDATE spell_chain SET next_spell='11671' WHERE spell_id = 7648;
UPDATE spell_chain SET next_spell='27187' WHERE spell_id = 26968;
UPDATE spell_chain SET next_spell='8058' WHERE spell_id = 8056;
UPDATE spell_chain SET next_spell='8450' WHERE spell_id = 604;
UPDATE spell_chain SET next_spell='10880' WHERE spell_id = 2010;
UPDATE spell_chain SET next_spell='13554' WHERE spell_id = 13553;
UPDATE spell_chain SET next_spell='10587' WHERE spell_id = 10586;
UPDATE spell_chain SET next_spell='27025' WHERE spell_id = 14317;
UPDATE spell_chain SET next_spell='42230' WHERE spell_id = 42233;
UPDATE spell_chain SET next_spell='6065' WHERE spell_id = 3747;
UPDATE spell_chain SET next_spell='602' WHERE spell_id = 7128;
UPDATE spell_chain SET next_spell='26797 26798 26801' WHERE spell_id = 12180;
UPDATE spell_chain SET next_spell='8018' WHERE spell_id = 8017;
UPDATE spell_chain SET next_spell='42245' WHERE spell_id = 42244;
UPDATE spell_chain SET next_spell='10220' WHERE spell_id = 10219;
UPDATE spell_chain SET next_spell='25477' WHERE spell_id = 19312;
UPDATE spell_chain SET next_spell='11711' WHERE spell_id = 6217;
UPDATE spell_chain SET next_spell='10890' WHERE spell_id = 10888;
UPDATE spell_chain SET next_spell='25299' WHERE spell_id = 9841;
UPDATE spell_chain SET next_spell='13555' WHERE spell_id = 13554;
UPDATE spell_chain SET next_spell='10955' WHERE spell_id = 9485;
UPDATE spell_chain SET next_spell='44208' WHERE spell_id = 44207;
UPDATE spell_chain SET next_spell='25422' WHERE spell_id = 10623;
UPDATE spell_chain SET next_spell='10447' WHERE spell_id = 8053;
UPDATE spell_chain SET next_spell='25384' WHERE spell_id = 15261;
UPDATE spell_chain SET next_spell='12525' WHERE spell_id = 12524;
UPDATE spell_chain SET next_spell='25233' WHERE spell_id = 10917;
UPDATE spell_chain SET next_spell='8102' WHERE spell_id = 8092;
UPDATE spell_chain SET next_spell='17392' WHERE spell_id = 17391;
UPDATE spell_chain SET next_spell='27016' WHERE spell_id = 25295;
UPDATE spell_chain SET next_spell='6074' WHERE spell_id = 139;
UPDATE spell_chain SET next_spell='782' WHERE spell_id = 467;
UPDATE spell_chain SET next_spell='17924' WHERE spell_id = 6353;
UPDATE spell_chain SET next_spell='20915' WHERE spell_id = 20375;
UPDATE spell_chain SET next_spell='15267' WHERE spell_id = 15266;
UPDATE spell_chain SET next_spell='34917' WHERE spell_id = 34916;
UPDATE spell_chain SET next_spell='27264' WHERE spell_id = 18881;
UPDATE spell_chain SET next_spell='10215' WHERE spell_id = 8423;
UPDATE spell_chain SET next_spell='11659' WHERE spell_id = 7641;
UPDATE spell_chain SET next_spell='8416' WHERE spell_id = 5145;
UPDATE spell_chain SET next_spell='11725' WHERE spell_id = 1098;
UPDATE spell_chain SET next_spell='25916' WHERE spell_id = 25782;
UPDATE spell_chain SET next_spell='27074' WHERE spell_id = 27073;
UPDATE spell_chain SET next_spell='10314' WHERE spell_id = 10313;
UPDATE spell_chain SET next_spell='11687' WHERE spell_id = 1456;
UPDATE spell_chain SET next_spell='26981' WHERE spell_id = 25299;
UPDATE spell_chain SET next_spell='18870' WHERE spell_id = 18869;
UPDATE spell_chain SET next_spell='14304' WHERE spell_id = 14303;
UPDATE spell_chain SET next_spell='20770' WHERE spell_id = 10881;
UPDATE spell_chain SET next_spell='20923' WHERE spell_id = 20922;
UPDATE spell_chain SET next_spell='25435' WHERE spell_id = 20770;
UPDATE spell_chain SET next_spell='10191' WHERE spell_id = 8495;
UPDATE spell_chain SET next_spell='17391' WHERE spell_id = 17390;
UPDATE spell_chain SET next_spell='25489' WHERE spell_id = 16342;
UPDATE spell_chain SET next_spell='27282' WHERE spell_id = 26969;
UPDATE spell_chain SET next_spell='25423' WHERE spell_id = 25422;
UPDATE spell_chain SET next_spell='20900' WHERE spell_id = 19434;
UPDATE spell_chain SET next_spell='27080' WHERE spell_id = 10202;
UPDATE spell_chain SET next_spell='11677' WHERE spell_id = 6219;
UPDATE spell_chain SET next_spell='27217' WHERE spell_id = 11675;
UPDATE spell_chain SET next_spell='27138' WHERE spell_id = 10314;
UPDATE spell_chain SET next_spell='3700' WHERE spell_id = 3699;
UPDATE spell_chain SET next_spell='10308' WHERE spell_id = 5589;
UPDATE spell_chain SET next_spell='9749' WHERE spell_id = 778;
UPDATE spell_chain SET next_spell='10927' WHERE spell_id = 6078;
UPDATE spell_chain SET next_spell='10964' WHERE spell_id = 10963;
UPDATE spell_chain SET next_spell='25533' WHERE spell_id = 10438;
UPDATE spell_chain SET next_spell='10328' WHERE spell_id = 3472;
UPDATE spell_chain SET next_spell='10960' WHERE spell_id = 996;
UPDATE spell_chain SET next_spell='647' WHERE spell_id = 639;
UPDATE spell_chain SET next_spell='22782' WHERE spell_id = 6117;
UPDATE spell_chain SET next_spell='19940' WHERE spell_id = 19939;
UPDATE spell_chain SET next_spell='27135' WHERE spell_id = 25292;
UPDATE spell_chain SET next_spell='27026' WHERE spell_id = 14315;
UPDATE spell_chain SET next_spell='11740' WHERE spell_id = 11739;
UPDATE spell_chain SET next_spell='1088' WHERE spell_id = 705;
UPDATE spell_chain SET next_spell='8691' WHERE spell_id = 8687;
UPDATE spell_chain SET next_spell='34916' WHERE spell_id = 34914;
UPDATE spell_chain SET next_spell='11734' WHERE spell_id = 11733;
UPDATE spell_chain SET next_spell='10622' WHERE spell_id = 1064;
UPDATE spell_chain SET next_spell='5186' WHERE spell_id = 5185;
UPDATE spell_chain SET next_spell='20116' WHERE spell_id = 26573;
UPDATE spell_chain SET next_spell='5234' WHERE spell_id = 6756;
UPDATE spell_chain SET next_spell='14284' WHERE spell_id = 14283;
UPDATE spell_chain SET next_spell='14321' WHERE spell_id = 14320;
UPDATE spell_chain SET next_spell='25367' WHERE spell_id = 10894;
UPDATE spell_chain SET next_spell='6226' WHERE spell_id = 5138;
UPDATE spell_chain SET next_spell='6219' WHERE spell_id = 5740;
UPDATE spell_chain SET next_spell='8124' WHERE spell_id = 8122;
UPDATE spell_chain SET next_spell='6077' WHERE spell_id = 6076;
UPDATE spell_chain SET next_spell='20610' WHERE spell_id = 20609;
UPDATE spell_chain SET next_spell='14288' WHERE spell_id = 2643;
UPDATE spell_chain SET next_spell='34865' WHERE spell_id = 34864;
UPDATE spell_chain SET next_spell='15630' WHERE spell_id = 15629;
UPDATE spell_chain SET next_spell='10585' WHERE spell_id = 8190;
UPDATE spell_chain SET next_spell='26986' WHERE spell_id = 25298;
UPDATE spell_chain SET next_spell='38692' WHERE spell_id = 27070;
UPDATE spell_chain SET next_spell='44207' WHERE spell_id = 44206;
UPDATE spell_chain SET next_spell='13020' WHERE spell_id = 13019;
UPDATE spell_chain SET next_spell='8249' WHERE spell_id = 8227;
UPDATE spell_chain SET next_spell='27219' WHERE spell_id = 11700;
UPDATE spell_chain SET next_spell='6041' WHERE spell_id = 943;
UPDATE spell_chain SET next_spell='10395' WHERE spell_id = 8005;
UPDATE spell_chain SET next_spell='3699' WHERE spell_id = 3698;
UPDATE spell_chain SET next_spell='11693' WHERE spell_id = 3700;
UPDATE spell_chain SET next_spell='2819' WHERE spell_id = 2818;
UPDATE spell_chain SET next_spell='14203' WHERE spell_id = 14202;
UPDATE spell_chain SET next_spell='10408' WHERE spell_id = 10407;
UPDATE spell_chain SET next_spell='24977' WHERE spell_id = 24976;
UPDATE spell_chain SET next_spell='18930' WHERE spell_id = 17962;
UPDATE spell_chain SET next_spell='3661' WHERE spell_id = 3111;
UPDATE spell_chain SET next_spell='25379' WHERE spell_id = 10876;
UPDATE spell_chain SET next_spell='3111' WHERE spell_id = 136;
UPDATE spell_chain SET next_spell='16341' WHERE spell_id = 16339;
UPDATE spell_chain SET next_spell='8910' WHERE spell_id = 3627;
UPDATE spell_chain SET next_spell='14303' WHERE spell_id = 14302;
UPDATE spell_chain SET next_spell='26995' WHERE spell_id = 9901;
UPDATE spell_chain SET next_spell='5177' WHERE spell_id = 5176;
UPDATE spell_chain SET next_spell='15262' WHERE spell_id = 14914;
UPDATE spell_chain SET next_spell='14295' WHERE spell_id = 14294;
UPDATE spell_chain SET next_spell='10326' WHERE spell_id = 5627;
UPDATE spell_chain SET next_spell='6778' WHERE spell_id = 5189;
UPDATE spell_chain SET next_spell='24975' WHERE spell_id = 24974;
UPDATE spell_chain SET next_spell='19978' WHERE spell_id = 19977;
UPDATE spell_chain SET next_spell='27068' WHERE spell_id = 24133;
UPDATE spell_chain SET next_spell='9889' WHERE spell_id = 9888;
UPDATE spell_chain SET next_spell='33736' WHERE spell_id = 24398;
UPDATE spell_chain SET next_spell='11733' WHERE spell_id = 1086;
UPDATE spell_chain SET next_spell='25297' WHERE spell_id = 9889;
UPDATE spell_chain SET next_spell='11713' WHERE spell_id = 11712;
UPDATE spell_chain SET next_spell='10202' WHERE spell_id = 10201;
UPDATE spell_chain SET next_spell='10656 10658 10660' WHERE spell_id = 10662;
UPDATE spell_chain SET next_spell='8437' WHERE spell_id = 1449;
UPDATE spell_chain SET next_spell='27009' WHERE spell_id = 17329;
UPDATE spell_chain SET next_spell='27212' WHERE spell_id = 11678;
UPDATE spell_chain SET next_spell='18657' WHERE spell_id = 2637;
UPDATE spell_chain SET next_spell='11660' WHERE spell_id = 11659;
UPDATE spell_chain SET next_spell='1075' WHERE spell_id = 782;
UPDATE spell_chain SET next_spell='14281' WHERE spell_id = 3044;
UPDATE spell_chain SET next_spell='600' WHERE spell_id = 592;
UPDATE spell_chain SET next_spell='27021' WHERE spell_id = 25294;
UPDATE spell_chain SET next_spell='30546' WHERE spell_id = 27263;
UPDATE spell_chain SET next_spell='18931' WHERE spell_id = 18930;
UPDATE spell_chain SET next_spell='27018' WHERE spell_id = 14280;
UPDATE spell_chain SET next_spell='28612' WHERE spell_id = 10145;
UPDATE spell_chain SET next_spell='10946' WHERE spell_id = 10945;
UPDATE spell_chain SET next_spell='14310' WHERE spell_id = 1499;
UPDATE spell_chain SET next_spell='27132' WHERE spell_id = 18809;
UPDATE spell_chain SET next_spell='10479' WHERE spell_id = 10478;
UPDATE spell_chain SET next_spell='2860' WHERE spell_id = 930;
UPDATE spell_chain SET next_spell='11665' WHERE spell_id = 2941;
UPDATE spell_chain SET next_spell='27147' WHERE spell_id = 20729;
UPDATE spell_chain SET next_spell='2941' WHERE spell_id = 1094;
UPDATE spell_chain SET next_spell='27045' WHERE spell_id = 20190;
UPDATE spell_chain SET next_spell='14290' WHERE spell_id = 14289;
UPDATE spell_chain SET next_spell='8401' WHERE spell_id = 8400;
UPDATE spell_chain SET next_spell='20929' WHERE spell_id = 20473;
UPDATE spell_chain SET next_spell='1461' WHERE spell_id = 1460;
UPDATE spell_chain SET next_spell='943' WHERE spell_id = 915;
UPDATE spell_chain SET next_spell='27168' WHERE spell_id = 20914;
UPDATE spell_chain SET next_spell='26992' WHERE spell_id = 9910;
UPDATE spell_chain SET next_spell='930' WHERE spell_id = 421;
UPDATE spell_chain SET next_spell='9474' WHERE spell_id = 9473;
UPDATE spell_chain SET next_spell='8905' WHERE spell_id = 6780;
UPDATE spell_chain SET next_spell='25298' WHERE spell_id = 9876;
UPDATE spell_chain SET next_spell='25311' WHERE spell_id = 11672;
UPDATE spell_chain SET next_spell='10472' WHERE spell_id = 8058;
UPDATE spell_chain SET next_spell='10329' WHERE spell_id = 10328;
UPDATE spell_chain SET next_spell='10199' WHERE spell_id = 10197;
UPDATE spell_chain SET next_spell='6780' WHERE spell_id = 5180;
UPDATE spell_chain SET next_spell='33405' WHERE spell_id = 27134;
UPDATE spell_chain SET next_spell='19941' WHERE spell_id = 19940;
UPDATE spell_chain SET next_spell='905' WHERE spell_id = 325;
UPDATE spell_chain SET next_spell='19281' WHERE spell_id = 9035;
UPDATE spell_chain SET next_spell='7322' WHERE spell_id = 837;
UPDATE spell_chain SET next_spell='10496' WHERE spell_id = 10495;
UPDATE spell_chain SET next_spell='18807' WHERE spell_id = 17314;
UPDATE spell_chain SET next_spell='10613' WHERE spell_id = 8512;
UPDATE spell_chain SET next_spell='2055' WHERE spell_id = 2054;
UPDATE spell_chain SET next_spell='16342' WHERE spell_id = 16341;
UPDATE spell_chain SET next_spell='10916' WHERE spell_id = 10915;
UPDATE spell_chain SET next_spell='2091' WHERE spell_id = 2090;
UPDATE spell_chain SET next_spell='19854' WHERE spell_id = 19853;
UPDATE spell_chain SET next_spell='1006' WHERE spell_id = 602;
UPDATE spell_chain SET next_spell='26994' WHERE spell_id = 20748;
UPDATE spell_chain SET next_spell='639' WHERE spell_id = 635;
UPDATE spell_chain SET next_spell='10432' WHERE spell_id = 10431;
UPDATE spell_chain SET next_spell='19836' WHERE spell_id = 19835;
UPDATE spell_chain SET next_spell='10958' WHERE spell_id = 10957;
UPDATE spell_chain SET next_spell='19279' WHERE spell_id = 19278;
UPDATE spell_chain SET next_spell='14316' WHERE spell_id = 13813;
UPDATE spell_chain SET next_spell='27145' WHERE spell_id = 25890;
UPDATE spell_chain SET next_spell='25420' WHERE spell_id = 10468;
UPDATE spell_chain SET next_spell='13224' WHERE spell_id = 13223;
UPDATE spell_chain SET next_spell='25560' WHERE spell_id = 10479;
UPDATE spell_chain SET next_spell='15430' WHERE spell_id = 15237;
UPDATE spell_chain SET next_spell='10894' WHERE spell_id = 10893;
UPDATE spell_chain SET next_spell='25918' WHERE spell_id = 25894;
UPDATE spell_chain SET next_spell='15207' WHERE spell_id = 10392;
UPDATE spell_chain SET next_spell='7128' WHERE spell_id = 588;
UPDATE spell_chain SET next_spell='28672 28675 28677' WHERE spell_id = 11611;
UPDATE spell_chain SET next_spell='10161' WHERE spell_id = 10160;
UPDATE spell_chain SET next_spell='25470' WHERE spell_id = 19285;
UPDATE spell_chain SET next_spell='38704' WHERE spell_id = 38699;
UPDATE spell_chain SET next_spell='14265' WHERE spell_id = 14264;
UPDATE spell_chain SET next_spell='1456' WHERE spell_id = 1455;
UPDATE spell_chain SET next_spell='10909' WHERE spell_id = 2096;
UPDATE spell_chain SET next_spell='25472' WHERE spell_id = 25469;
UPDATE spell_chain SET next_spell='11335' WHERE spell_id = 8689;
UPDATE spell_chain SET next_spell='15112' WHERE spell_id = 15111;
UPDATE spell_chain SET next_spell='13543' WHERE spell_id = 13542;
UPDATE spell_chain SET next_spell='205' WHERE spell_id = 116;
UPDATE spell_chain SET next_spell='19275' WHERE spell_id = 19274;
UPDATE spell_chain SET next_spell='20729' WHERE spell_id = 6940;
UPDATE spell_chain SET next_spell='8044' WHERE spell_id = 8042;
UPDATE spell_chain SET next_spell='26968' WHERE spell_id = 25349;
UPDATE spell_chain SET next_spell='695' WHERE spell_id = 686;
UPDATE spell_chain SET next_spell='10486' WHERE spell_id = 8235;
UPDATE spell_chain SET next_spell='14317' WHERE spell_id = 14316;
UPDATE spell_chain SET next_spell='14266' WHERE spell_id = 14265;
UPDATE spell_chain SET next_spell='17329' WHERE spell_id = 16813;
UPDATE spell_chain SET next_spell='8461' WHERE spell_id = 6143;
UPDATE spell_chain SET next_spell='8413' WHERE spell_id = 8412;
UPDATE spell_chain SET next_spell='20910' WHERE spell_id = 20909;
UPDATE spell_chain SET next_spell='10888' WHERE spell_id = 8124;
UPDATE spell_chain SET next_spell='20609' WHERE spell_id = 2008;
UPDATE spell_chain SET next_spell='15632' WHERE spell_id = 15631;
UPDATE spell_chain SET next_spell='27067' WHERE spell_id = 20910;
UPDATE spell_chain SET next_spell='17922' WHERE spell_id = 17921;
UPDATE spell_chain SET next_spell='14262' WHERE spell_id = 14261;
UPDATE spell_chain SET next_spell='25485' WHERE spell_id = 25479;
UPDATE spell_chain SET next_spell='10463' WHERE spell_id = 10462;
UPDATE spell_chain SET next_spell='10466' WHERE spell_id = 8010;
UPDATE spell_chain SET next_spell='11695' WHERE spell_id = 11694;
UPDATE spell_chain SET next_spell='25446' WHERE spell_id = 19305;
UPDATE spell_chain SET next_spell='5615' WHERE spell_id = 5614;
UPDATE spell_chain SET next_spell='25387' WHERE spell_id = 18807;
UPDATE spell_chain SET next_spell='10537' WHERE spell_id = 8184;
UPDATE spell_chain SET next_spell='24274' WHERE spell_id = 24275;
UPDATE spell_chain SET next_spell='10149' WHERE spell_id = 10148;
UPDATE spell_chain SET next_spell='19261' WHERE spell_id = 2652;
UPDATE spell_chain SET next_spell='14279' WHERE spell_id = 3034;
UPDATE spell_chain SET next_spell='13031' WHERE spell_id = 11426;
UPDATE spell_chain SET next_spell='10413' WHERE spell_id = 10412;
UPDATE spell_chain SET next_spell='14270' WHERE spell_id = 14269;
UPDATE spell_chain SET next_spell='14263' WHERE spell_id = 14262;
UPDATE spell_chain SET next_spell='1106' WHERE spell_id = 1088;
UPDATE spell_chain SET next_spell='24976' WHERE spell_id = 24975;
UPDATE spell_chain SET next_spell='8949' WHERE spell_id = 2912;
UPDATE spell_chain SET next_spell='8950' WHERE spell_id = 8949;
UPDATE spell_chain SET next_spell='11342' WHERE spell_id = 11341;
UPDATE spell_chain SET next_spell='8417' WHERE spell_id = 8416;
UPDATE spell_chain SET next_spell='8103' WHERE spell_id = 8102;
UPDATE spell_chain SET next_spell='27131' WHERE spell_id = 10193;
UPDATE spell_chain SET next_spell='5614' WHERE spell_id = 879;
UPDATE spell_chain SET next_spell='2010' WHERE spell_id = 2006;
UPDATE spell_chain SET next_spell='5189' WHERE spell_id = 5188;
UPDATE spell_chain SET next_spell='20773' WHERE spell_id = 20772;
UPDATE spell_chain SET next_spell='9750' WHERE spell_id = 8941;
UPDATE spell_chain SET next_spell='10211' WHERE spell_id = 8417;
UPDATE spell_chain SET next_spell='2090' WHERE spell_id = 1430;
UPDATE spell_chain SET next_spell='9853' WHERE spell_id = 9852;
UPDATE spell_chain SET next_spell='8907' WHERE spell_id = 5234;
UPDATE spell_chain SET next_spell='6375' WHERE spell_id = 5394;
UPDATE spell_chain SET next_spell='25213' WHERE spell_id = 25210;
UPDATE spell_chain SET next_spell='10186' WHERE spell_id = 10185;
UPDATE spell_chain SET next_spell='8105' WHERE spell_id = 8104;
UPDATE spell_chain SET next_spell='10322' WHERE spell_id = 7328;
UPDATE spell_chain SET next_spell='10458' WHERE spell_id = 8037;
UPDATE spell_chain SET next_spell='20904' WHERE spell_id = 20903;
UPDATE spell_chain SET next_spell='20747' WHERE spell_id = 20742;
UPDATE spell_chain SET next_spell='11683' WHERE spell_id = 1949;
UPDATE spell_chain SET next_spell='990' WHERE spell_id = 597;
UPDATE spell_chain SET next_spell='14272' WHERE spell_id = 781;
UPDATE spell_chain SET next_spell='25361' WHERE spell_id = 10442;
UPDATE spell_chain SET next_spell='27144' WHERE spell_id = 19979;
UPDATE spell_chain SET next_spell='1460' WHERE spell_id = 1459;
UPDATE spell_chain SET next_spell='14261' WHERE spell_id = 14260;
UPDATE spell_chain SET next_spell='19299' WHERE spell_id = 19296;
UPDATE spell_chain SET next_spell='11730' WHERE spell_id = 11729;
UPDATE spell_chain SET next_spell='27125' WHERE spell_id = 22783;
UPDATE spell_chain SET next_spell='10614' WHERE spell_id = 10613;
UPDATE spell_chain SET next_spell='10427' WHERE spell_id = 6392;
UPDATE spell_chain SET next_spell='8005' WHERE spell_id = 959;
UPDATE spell_chain SET next_spell='14311' WHERE spell_id = 14310;
UPDATE spell_chain SET next_spell='11672' WHERE spell_id = 11671;
UPDATE spell_chain SET next_spell='709' WHERE spell_id = 699;
UPDATE spell_chain SET next_spell='19280' WHERE spell_id = 19279;
UPDATE spell_chain SET next_spell='11688' WHERE spell_id = 11687;
UPDATE spell_chain SET next_spell='10933' WHERE spell_id = 6060;
UPDATE spell_chain SET next_spell='6756' WHERE spell_id = 5232;
UPDATE spell_chain SET next_spell='1026' WHERE spell_id = 647;
UPDATE spell_chain SET next_spell='11667' WHERE spell_id = 11665;
UPDATE spell_chain SET next_spell='27124' WHERE spell_id = 10220;
UPDATE spell_chain SET next_spell='16352' WHERE spell_id = 10458;
UPDATE spell_chain SET next_spell='25221' WHERE spell_id = 25315;
UPDATE spell_chain SET next_spell='19265' WHERE spell_id = 19264;
UPDATE spell_chain SET next_spell='992' WHERE spell_id = 970;
UPDATE spell_chain SET next_spell='9858' WHERE spell_id = 9857;
UPDATE spell_chain SET next_spell='14323' WHERE spell_id = 1130;
UPDATE spell_chain SET next_spell='13223' WHERE spell_id = 13222;
UPDATE spell_chain SET next_spell='13033' WHERE spell_id = 13032;
UPDATE spell_chain SET next_spell='18868' WHERE spell_id = 18867;
UPDATE spell_chain SET next_spell='10392' WHERE spell_id = 10391;
UPDATE spell_chain SET next_spell='3627' WHERE spell_id = 2091;
UPDATE spell_chain SET next_spell='8914' WHERE spell_id = 1075;
UPDATE spell_chain SET next_spell='27259' WHERE spell_id = 11695;
UPDATE spell_chain SET next_spell='10148' WHERE spell_id = 8402;
UPDATE spell_chain SET next_spell='25439' WHERE spell_id = 10605;
UPDATE spell_chain SET next_spell='27799' WHERE spell_id = 15431;
UPDATE spell_chain SET next_spell='10462' WHERE spell_id = 6377;
UPDATE spell_chain SET next_spell='20742' WHERE spell_id = 20739;
UPDATE spell_chain SET next_spell='27180' WHERE spell_id = 24239;
UPDATE spell_chain SET next_spell='10391' WHERE spell_id = 6041;
UPDATE spell_chain SET next_spell='8444' WHERE spell_id = 2948;
UPDATE spell_chain SET next_spell='25574' WHERE spell_id = 10601;
UPDATE spell_chain SET next_spell='25312' WHERE spell_id = 27841;
UPDATE spell_chain SET next_spell='3698' WHERE spell_id = 755;
UPDATE spell_chain SET next_spell='19278' WHERE spell_id = 19277;
UPDATE spell_chain SET next_spell='8412' WHERE spell_id = 2138;
UPDATE spell_chain SET next_spell='1014' WHERE spell_id = 980;
UPDATE spell_chain SET next_spell='15263' WHERE spell_id = 15262;
UPDATE spell_chain SET next_spell='8455' WHERE spell_id = 1008;
UPDATE spell_chain SET next_spell='1455' WHERE spell_id = 1454;
UPDATE spell_chain SET next_spell='10601' WHERE spell_id = 10600;
UPDATE spell_chain SET next_spell='14294' WHERE spell_id = 1510;
UPDATE spell_chain SET next_spell='10399' WHERE spell_id = 8019;
UPDATE spell_chain SET next_spell='27263' WHERE spell_id = 18871;
UPDATE spell_chain SET next_spell='27169' WHERE spell_id = 25899;
UPDATE spell_chain SET next_spell='11712' WHERE spell_id = 11711;
UPDATE spell_chain SET next_spell='14325' WHERE spell_id = 14324;
UPDATE spell_chain SET next_spell='20922' WHERE spell_id = 20116;
UPDATE spell_chain SET next_spell='14299' WHERE spell_id = 14298;
UPDATE spell_chain SET next_spell='16314' WHERE spell_id = 10399;
UPDATE spell_chain SET next_spell='20357' WHERE spell_id = 20356;
UPDATE spell_chain SET next_spell='27141' WHERE spell_id = 25916;
UPDATE spell_chain SET next_spell='19303' WHERE spell_id = 19302;
UPDATE spell_chain SET next_spell='19939' WHERE spell_id = 19750;
UPDATE spell_chain SET next_spell='16810' WHERE spell_id = 16689;
UPDATE spell_chain SET next_spell='1094' WHERE spell_id = 707;
UPDATE spell_chain SET next_spell='9885' WHERE spell_id = 9884;
UPDATE spell_chain SET next_spell='20356' WHERE spell_id = 20166;
UPDATE spell_chain SET next_spell='7659' WHERE spell_id = 7658;
UPDATE spell_chain SET next_spell='1042' WHERE spell_id = 1026;
UPDATE spell_chain SET next_spell='10957' WHERE spell_id = 976;
UPDATE spell_chain SET next_spell='32996' WHERE spell_id = 32379;
UPDATE spell_chain SET next_spell='11678' WHERE spell_id = 11677;
UPDATE spell_chain SET next_spell='20927' WHERE spell_id = 20925;
UPDATE spell_chain SET next_spell='14819' WHERE spell_id = 14818;
UPDATE spell_chain SET next_spell='20293' WHERE spell_id = 20292;
UPDATE spell_chain SET next_spell='25210' WHERE spell_id = 25314;
UPDATE spell_chain SET next_spell='6217' WHERE spell_id = 1014;
UPDATE spell_chain SET next_spell='27128' WHERE spell_id = 10225;
UPDATE spell_chain SET next_spell='27266' WHERE spell_id = 18932;
UPDATE spell_chain SET next_spell='27216' WHERE spell_id = 25311;
UPDATE spell_chain SET next_spell='24132' WHERE spell_id = 19386;
UPDATE spell_chain SET next_spell='25217' WHERE spell_id = 10901;
UPDATE spell_chain SET next_spell='8689' WHERE spell_id = 8685;
UPDATE spell_chain SET next_spell='16811' WHERE spell_id = 16810;
UPDATE spell_chain SET next_spell='26987' WHERE spell_id = 9835;
UPDATE spell_chain SET next_spell='27022' WHERE spell_id = 14295;
UPDATE spell_chain SET next_spell='8446' WHERE spell_id = 8445;
UPDATE spell_chain SET next_spell='25331' WHERE spell_id = 27801;
UPDATE spell_chain SET next_spell='20901' WHERE spell_id = 20900;
UPDATE spell_chain SET next_spell='24133' WHERE spell_id = 24132;
UPDATE spell_chain SET next_spell='19838' WHERE spell_id = 19837;
UPDATE spell_chain SET next_spell='27283' WHERE spell_id = 13230;
UPDATE spell_chain SET next_spell='14300' WHERE spell_id = 14299;
UPDATE spell_chain SET next_spell='27012' WHERE spell_id = 17402;
UPDATE spell_chain SET next_spell='14305' WHERE spell_id = 14304;
UPDATE spell_chain SET next_spell='14289' WHERE spell_id = 14288;
UPDATE spell_chain SET next_spell='707' WHERE spell_id = 348;
UPDATE spell_chain SET next_spell='18879' WHERE spell_id = 18265;
UPDATE spell_chain SET next_spell='8457' WHERE spell_id = 543;
UPDATE spell_chain SET next_spell='25448' WHERE spell_id = 15208;
UPDATE spell_chain SET next_spell='5232' WHERE spell_id = 1126;
UPDATE spell_chain SET next_spell='26983' WHERE spell_id = 9863;
UPDATE spell_chain SET next_spell='10181' WHERE spell_id = 10180;
UPDATE spell_chain SET next_spell='14318' WHERE spell_id = 13165;
UPDATE spell_chain SET next_spell='10538' WHERE spell_id = 10537;
UPDATE spell_chain SET next_spell='10414' WHERE spell_id = 10413;
UPDATE spell_chain SET next_spell='19311' WHERE spell_id = 19310;
UPDATE spell_chain SET next_spell='11343' WHERE spell_id = 11342;
UPDATE spell_chain SET next_spell='30414' WHERE spell_id = 30413;
UPDATE spell_chain SET next_spell='25396' WHERE spell_id = 25391;
UPDATE spell_chain SET next_spell='32699' WHERE spell_id = 31935;
UPDATE spell_chain SET next_spell='26890' WHERE spell_id = 11337;
UPDATE spell_chain SET next_spell='16362' WHERE spell_id = 10486;
UPDATE spell_chain SET next_spell='17312' WHERE spell_id = 17311;
UPDATE spell_chain SET next_spell='10201' WHERE spell_id = 8439;
UPDATE spell_chain SET next_spell='18871' WHERE spell_id = 18870;
UPDATE spell_chain SET next_spell='13551' WHERE spell_id = 13550;
UPDATE spell_chain SET next_spell='25508' WHERE spell_id = 10408;
UPDATE spell_chain SET next_spell='13018' WHERE spell_id = 11113;
UPDATE spell_chain SET next_spell='1058' WHERE spell_id = 774;
UPDATE spell_chain SET next_spell='939' WHERE spell_id = 913;
UPDATE spell_chain SET next_spell='27133' WHERE spell_id = 13021;
UPDATE spell_chain SET next_spell='27222' WHERE spell_id = 11689;
UPDATE spell_chain SET next_spell='27013' WHERE spell_id = 24977;
UPDATE spell_chain SET next_spell='33946' WHERE spell_id = 27130;
UPDATE spell_chain SET next_spell='16387' WHERE spell_id = 10526;
UPDATE spell_chain SET next_spell='16813' WHERE spell_id = 16812;
UPDATE spell_chain SET next_spell='5599' WHERE spell_id = 1022;
UPDATE spell_chain SET next_spell='13550' WHERE spell_id = 13549;
UPDATE spell_chain SET next_spell='13229' WHERE spell_id = 13228;
UPDATE spell_chain SET next_spell='5144' WHERE spell_id = 5143;
UPDATE spell_chain SET next_spell='9884' WHERE spell_id = 8907;
UPDATE spell_chain SET next_spell='7641' WHERE spell_id = 1106;
UPDATE spell_chain SET next_spell='9862' WHERE spell_id = 8918;
UPDATE spell_chain SET next_spell='14315' WHERE spell_id = 14314;
UPDATE spell_chain SET next_spell='17390' WHERE spell_id = 16857;
UPDATE spell_chain SET next_spell='42232' WHERE spell_id = 42231;
UPDATE spell_chain SET next_spell='20305' WHERE spell_id = 20162;
UPDATE spell_chain SET next_spell='27209' WHERE spell_id = 25307;
UPDATE spell_chain SET next_spell='19241' WHERE spell_id = 19240;
UPDATE spell_chain SET next_spell='19942' WHERE spell_id = 19941;
UPDATE spell_chain SET next_spell='8929' WHERE spell_id = 8928;
UPDATE spell_chain SET next_spell='30910' WHERE spell_id = 603;
UPDATE spell_chain SET next_spell='10313' WHERE spell_id = 10312;
UPDATE spell_chain SET next_spell='14285' WHERE spell_id = 14284;
UPDATE spell_chain SET next_spell='16339' WHERE spell_id = 8030;
UPDATE spell_chain SET next_spell='13542' WHERE spell_id = 3662;
UPDATE spell_chain SET next_spell='14322' WHERE spell_id = 14321;
UPDATE spell_chain SET next_spell='17926' WHERE spell_id = 17925;
UPDATE spell_chain SET next_spell='37420' WHERE spell_id = 10140;
UPDATE spell_chain SET next_spell='36916' WHERE spell_id = 14271;
UPDATE spell_chain SET next_spell='19285' WHERE spell_id = 19284;
UPDATE spell_chain SET next_spell='8940' WHERE spell_id = 8939;
UPDATE spell_chain SET next_spell='25347' WHERE spell_id = 11358;
UPDATE spell_chain SET next_spell='6371' WHERE spell_id = 5672;
UPDATE spell_chain SET next_spell='15261' WHERE spell_id = 15267;
UPDATE spell_chain SET next_spell='10278' WHERE spell_id = 5599;
UPDATE spell_chain SET next_spell='11708' WHERE spell_id = 11707;
UPDATE spell_chain SET next_spell='10461' WHERE spell_id = 10460;
UPDATE spell_chain SET next_spell='20291' WHERE spell_id = 20290;
UPDATE spell_chain SET next_spell='27011' WHERE spell_id = 17392;
UPDATE spell_chain SET next_spell='18647' WHERE spell_id = 710;
UPDATE spell_chain SET next_spell='10205' WHERE spell_id = 8446;
UPDATE spell_chain SET next_spell='11722' WHERE spell_id = 11721;
UPDATE spell_chain SET next_spell='9473' WHERE spell_id = 9472;
UPDATE spell_chain SET next_spell='16812' WHERE spell_id = 16811;
UPDATE spell_chain SET next_spell='5589' WHERE spell_id = 5588;
UPDATE spell_chain SET next_spell='8288' WHERE spell_id = 1120;
UPDATE spell_chain SET next_spell='7658' WHERE spell_id = 704;
UPDATE spell_chain SET next_spell='27086' WHERE spell_id = 10216;
UPDATE spell_chain SET next_spell='11357' WHERE spell_id = 2837;
UPDATE spell_chain SET next_spell='28610' WHERE spell_id = 11740;
UPDATE spell_chain SET next_spell='8160' WHERE spell_id = 8075;
UPDATE spell_chain SET next_spell='34864' WHERE spell_id = 34863;
UPDATE spell_chain SET next_spell='14326' WHERE spell_id = 1513;
UPDATE spell_chain SET next_spell='1086' WHERE spell_id = 706;
UPDATE spell_chain SET next_spell='20308' WHERE spell_id = 20307;
UPDATE spell_chain SET next_spell='25306' WHERE spell_id = 10151;
UPDATE spell_chain SET next_spell='10881' WHERE spell_id = 10880;
UPDATE spell_chain SET next_spell='25563' WHERE spell_id = 10538;
UPDATE spell_chain SET next_spell='6066' WHERE spell_id = 6065;
UPDATE spell_chain SET next_spell='2053' WHERE spell_id = 2052;
UPDATE spell_chain SET next_spell='39374' WHERE spell_id = 27683;
UPDATE spell_chain SET next_spell='9787 9788' WHERE spell_id = 9785;
UPDATE spell_chain SET next_spell='8445' WHERE spell_id = 8444;
UPDATE spell_chain SET next_spell='42244' WHERE spell_id = 42243;
UPDATE spell_chain SET next_spell='10173' WHERE spell_id = 8451;
UPDATE spell_chain SET next_spell='12505' WHERE spell_id = 11366;
UPDATE spell_chain SET next_spell='17311' WHERE spell_id = 15407;
UPDATE spell_chain SET next_spell='27046' WHERE spell_id = 13544;
UPDATE spell_chain SET next_spell='11314' WHERE spell_id = 8499;
UPDATE spell_chain SET next_spell='6377' WHERE spell_id = 6375;
UPDATE spell_chain SET next_spell='20919' WHERE spell_id = 20918;
UPDATE spell_chain SET next_spell='10192' WHERE spell_id = 10191;
UPDATE spell_chain SET next_spell='25308' WHERE spell_id = 25316;
UPDATE spell_chain SET next_spell='3662' WHERE spell_id = 3661;
UPDATE spell_chain SET next_spell='27174' WHERE spell_id = 20930;
UPDATE spell_chain SET next_spell='9833' WHERE spell_id = 8929;
UPDATE spell_chain SET next_spell='7300' WHERE spell_id = 168;
UPDATE spell_chain SET next_spell='27126' WHERE spell_id = 10157;
UPDATE spell_chain SET next_spell='11337' WHERE spell_id = 11336;
UPDATE spell_chain SET next_spell='28172' WHERE spell_id = 17728;
UPDATE spell_chain SET next_spell='8104' WHERE spell_id = 8103;
UPDATE spell_chain SET next_spell='10937' WHERE spell_id = 2791;
UPDATE spell_chain SET next_spell='8685' WHERE spell_id = 8680;
UPDATE spell_chain SET next_spell='19850' WHERE spell_id = 19742;
UPDATE spell_chain SET next_spell='17923' WHERE spell_id = 17922;
UPDATE spell_chain SET next_spell='17402' WHERE spell_id = 17401;
UPDATE spell_chain SET next_spell='27260' WHERE spell_id = 11735;
UPDATE spell_chain SET next_spell='9910' WHERE spell_id = 9756;
UPDATE spell_chain SET next_spell='8106' WHERE spell_id = 8105;
UPDATE spell_chain SET next_spell='10150' WHERE spell_id = 10149;
UPDATE spell_chain SET next_spell='10169' WHERE spell_id = 8455;
UPDATE spell_chain SET next_spell='25380' WHERE spell_id = 25379;
UPDATE spell_chain SET next_spell='25547' WHERE spell_id = 25546;
UPDATE spell_chain SET next_spell='25437' WHERE spell_id = 19243;
UPDATE spell_chain SET next_spell='6372' WHERE spell_id = 6371;
UPDATE spell_chain SET next_spell='27155' WHERE spell_id = 20293;
UPDATE spell_chain SET next_spell='10929' WHERE spell_id = 10928;
UPDATE spell_chain SET next_spell='19277' WHERE spell_id = 19276;
UPDATE spell_chain SET next_spell='10156' WHERE spell_id = 1461;
UPDATE spell_chain SET next_spell='14282' WHERE spell_id = 14281;
UPDATE spell_chain SET next_spell='11661' WHERE spell_id = 11660;
UPDATE spell_chain SET next_spell='27229' WHERE spell_id = 17937;
UPDATE spell_chain SET next_spell='20903' WHERE spell_id = 20902;
UPDATE spell_chain SET next_spell='8408' WHERE spell_id = 8407;
UPDATE spell_chain SET next_spell='27224' WHERE spell_id = 11708;
UPDATE spell_chain SET next_spell='14269' WHERE spell_id = 1495;
UPDATE spell_chain SET next_spell='14271' WHERE spell_id = 14270;
UPDATE spell_chain SET next_spell='6076' WHERE spell_id = 6075;
UPDATE spell_chain SET next_spell='19283' WHERE spell_id = 19282;
UPDATE spell_chain SET next_spell='17919' WHERE spell_id = 5676;
UPDATE spell_chain SET next_spell='20918' WHERE spell_id = 20915;
UPDATE spell_chain SET next_spell='988' WHERE spell_id = 527;
UPDATE spell_chain SET next_spell='18938' WHERE spell_id = 18937;
UPDATE spell_chain SET next_spell='26985' WHERE spell_id = 26984;
UPDATE spell_chain SET next_spell='5573' WHERE spell_id = 498;
UPDATE spell_chain SET next_spell='26979' WHERE spell_id = 26978;
UPDATE spell_chain SET next_spell='27090' WHERE spell_id = 37420;
UPDATE spell_chain SET next_spell='11694' WHERE spell_id = 11693;
UPDATE spell_chain SET next_spell='8008' WHERE spell_id = 8004;
UPDATE spell_chain SET next_spell='778' WHERE spell_id = 770;
UPDATE spell_chain SET next_spell='27023' WHERE spell_id = 14305;
UPDATE spell_chain SET next_spell='10900' WHERE spell_id = 10899;
UPDATE spell_chain SET next_spell='20739' WHERE spell_id = 20484;
UPDATE spell_chain SET next_spell='25363' WHERE spell_id = 10934;
UPDATE spell_chain SET next_spell='5178' WHERE spell_id = 5177;
UPDATE spell_chain SET next_spell='6063' WHERE spell_id = 2055;
UPDATE spell_chain SET next_spell='27189' WHERE spell_id = 13224;
UPDATE spell_chain SET next_spell='16355' WHERE spell_id = 10456;
UPDATE spell_chain SET next_spell='27079' WHERE spell_id = 27078;
UPDATE spell_chain SET next_spell='5505' WHERE spell_id = 5504;
UPDATE spell_chain SET next_spell='30912' WHERE spell_id = 27266;
UPDATE spell_chain SET next_spell='11707' WHERE spell_id = 7646;
UPDATE spell_chain SET next_spell='33938' WHERE spell_id = 27132;
UPDATE spell_chain SET next_spell='38699' WHERE spell_id = 27075;
UPDATE spell_chain SET next_spell='12826' WHERE spell_id = 12825;
UPDATE spell_chain SET next_spell='25314' WHERE spell_id = 10965;
UPDATE spell_chain SET next_spell='27158' WHERE spell_id = 20308;
UPDATE spell_chain SET next_spell='9834' WHERE spell_id = 9833;
UPDATE spell_chain SET next_spell='25469' WHERE spell_id = 10432;
UPDATE spell_chain SET next_spell='8037' WHERE spell_id = 8034;
UPDATE spell_chain SET next_spell='27073' WHERE spell_id = 10207;
UPDATE spell_chain SET next_spell='25577' WHERE spell_id = 15112;
UPDATE spell_chain SET next_spell='27019' WHERE spell_id = 14287;
UPDATE spell_chain SET next_spell='9875' WHERE spell_id = 8951;
UPDATE spell_chain SET next_spell='8423' WHERE spell_id = 8422;
UPDATE spell_chain SET next_spell='11735' WHERE spell_id = 11734;
UPDATE spell_chain SET next_spell='10212' WHERE spell_id = 10211;
UPDATE spell_chain SET next_spell='9856' WHERE spell_id = 9750;
UPDATE spell_chain SET next_spell='10174' WHERE spell_id = 10173;
UPDATE spell_chain SET next_spell='19296' WHERE spell_id = 10797;
UPDATE spell_chain SET next_spell='14274' WHERE spell_id = 20736;
UPDATE spell_chain SET next_spell='19262' WHERE spell_id = 19261;
UPDATE spell_chain SET next_spell='14327' WHERE spell_id = 14326;
UPDATE spell_chain SET next_spell='6202' WHERE spell_id = 6201;
UPDATE spell_chain SET next_spell='27142' WHERE spell_id = 25290;
UPDATE spell_chain SET next_spell='27870' WHERE spell_id = 724;
UPDATE spell_chain SET next_spell='9485' WHERE spell_id = 9484;
UPDATE spell_chain SET next_spell='9840' WHERE spell_id = 9839;
UPDATE spell_chain SET next_spell='17920' WHERE spell_id = 17919;
UPDATE spell_chain SET next_spell='2767' WHERE spell_id = 992;
UPDATE spell_chain SET next_spell='25566' WHERE spell_id = 10461;
UPDATE spell_chain SET next_spell='27082' WHERE spell_id = 27080;
UPDATE spell_chain SET next_spell='8053' WHERE spell_id = 8052;
UPDATE spell_chain SET next_spell='27143' WHERE spell_id = 25918;
UPDATE spell_chain SET next_spell='13032' WHERE spell_id = 13031;
UPDATE spell_chain SET next_spell='11358' WHERE spell_id = 11357;
UPDATE spell_chain SET next_spell='10396' WHERE spell_id = 10395;
UPDATE spell_chain SET next_spell='33041' WHERE spell_id = 31661;
UPDATE spell_chain SET next_spell='10177' WHERE spell_id = 8462;
UPDATE spell_chain SET next_spell='33717' WHERE spell_id = 28612;
UPDATE spell_chain SET next_spell='25596' WHERE spell_id = 10953;
UPDATE spell_chain SET next_spell='5699' WHERE spell_id = 6202;
UPDATE spell_chain SET next_spell='1004' WHERE spell_id = 984;
UPDATE spell_chain SET next_spell='25307' WHERE spell_id = 11661;
UPDATE spell_chain SET next_spell='42233' WHERE spell_id = 42232;
UPDATE spell_chain SET next_spell='15111' WHERE spell_id = 15107;
UPDATE spell_chain SET next_spell='11353' WHERE spell_id = 2819;
UPDATE spell_chain SET next_spell='10193' WHERE spell_id = 10192;
UPDATE spell_chain SET next_spell='8010' WHERE spell_id = 8008;
UPDATE spell_chain SET next_spell='8406' WHERE spell_id = 7322;
UPDATE spell_chain SET next_spell='16315' WHERE spell_id = 16314;
UPDATE spell_chain SET next_spell='11684' WHERE spell_id = 11683;
UPDATE spell_chain SET next_spell='9912' WHERE spell_id = 8905;
UPDATE spell_chain SET next_spell='16316' WHERE spell_id = 16315;
UPDATE spell_chain SET next_spell='8694' WHERE spell_id = 5763;
UPDATE spell_chain SET next_spell='20776' WHERE spell_id = 20610;
UPDATE spell_chain SET next_spell='11336' WHERE spell_id = 11335;
UPDATE spell_chain SET next_spell='44205' WHERE spell_id = 44203;
UPDATE spell_chain SET next_spell='10627' WHERE spell_id = 8835;
UPDATE spell_chain SET next_spell='25441' WHERE spell_id = 19275;
UPDATE spell_chain SET next_spell='17928' WHERE spell_id = 5484;
UPDATE spell_chain SET next_spell='11729' WHERE spell_id = 5699;
UPDATE spell_chain SET next_spell='25454' WHERE spell_id = 10414;
UPDATE spell_chain SET next_spell='10497' WHERE spell_id = 10496;
UPDATE spell_chain SET next_spell='25500' WHERE spell_id = 16356;
UPDATE spell_chain SET next_spell='13019' WHERE spell_id = 13018;
UPDATE spell_chain SET next_spell='10448' WHERE spell_id = 10447;
UPDATE spell_chain SET next_spell='13222' WHERE spell_id = 13218;
UPDATE spell_chain SET next_spell='8046' WHERE spell_id = 8045;
UPDATE spell_chain SET next_spell='2837' WHERE spell_id = 2835;
UPDATE spell_chain SET next_spell='25392' WHERE spell_id = 21564;
UPDATE spell_chain SET next_spell='8925' WHERE spell_id = 8924;
UPDATE spell_chain SET next_spell='9472' WHERE spell_id = 2061;
UPDATE spell_chain SET next_spell='143' WHERE spell_id = 133;
UPDATE spell_chain SET next_spell='20913' WHERE spell_id = 20912;
UPDATE spell_chain SET next_spell='7651' WHERE spell_id = 709;
UPDATE spell_chain SET next_spell='33072' WHERE spell_id = 27174;
UPDATE spell_chain SET next_spell='8918' WHERE spell_id = 740;
UPDATE spell_chain SET next_spell='8458' WHERE spell_id = 8457;
UPDATE spell_chain SET next_spell='696' WHERE spell_id = 687;
UPDATE spell_chain SET next_spell='24239' WHERE spell_id = 24274;
UPDATE spell_chain SET next_spell='10942' WHERE spell_id = 10941;
UPDATE spell_chain SET next_spell='20902' WHERE spell_id = 20901;
UPDATE spell_chain SET next_spell='14301' WHERE spell_id = 14300;
UPDATE spell_chain SET next_spell='10160' WHERE spell_id = 10159;
UPDATE spell_chain SET next_spell='3140' WHERE spell_id = 145;
UPDATE spell_chain SET next_spell='25509' WHERE spell_id = 25508;
UPDATE spell_chain SET next_spell='25294' WHERE spell_id = 14290;
UPDATE spell_chain SET next_spell='25316' WHERE spell_id = 10961;
UPDATE spell_chain SET next_spell='27211' WHERE spell_id = 17924;
UPDATE spell_chain SET next_spell='865' WHERE spell_id = 122;
UPDATE spell_chain SET next_spell='10874' WHERE spell_id = 8131;
UPDATE spell_chain SET next_spell='19264' WHERE spell_id = 19262;
UPDATE spell_chain SET next_spell='11721' WHERE spell_id = 1490;
UPDATE spell_chain SET next_spell='6215' WHERE spell_id = 6213;
UPDATE spell_chain SET next_spell='27127' WHERE spell_id = 23028;
UPDATE spell_chain SET next_spell='8939' WHERE spell_id = 8938;
UPDATE spell_chain SET next_spell='25218' WHERE spell_id = 25217;
UPDATE spell_chain SET next_spell='8451' WHERE spell_id = 8450;
UPDATE spell_chain SET next_spell='27213' WHERE spell_id = 11684;
UPDATE spell_chain SET next_spell='10915' WHERE spell_id = 9474;
UPDATE spell_chain SET next_spell='594' WHERE spell_id = 589;
UPDATE spell_chain SET next_spell='10928' WHERE spell_id = 10927;
UPDATE spell_chain SET next_spell='8955' WHERE spell_id = 2908;
UPDATE spell_chain SET next_spell='25431' WHERE spell_id = 10952;
UPDATE spell_chain SET next_spell='325' WHERE spell_id = 324;
UPDATE spell_chain SET next_spell='20289' WHERE spell_id = 20288;
UPDATE spell_chain SET next_spell='12522' WHERE spell_id = 12505;
UPDATE spell_chain SET next_spell='2138' WHERE spell_id = 2137;
UPDATE spell_chain SET next_spell='14201' WHERE spell_id = 12880;
UPDATE spell_chain SET next_spell='27085' WHERE spell_id = 10187;
UPDATE spell_chain SET next_spell='8492' WHERE spell_id = 120;
UPDATE spell_chain SET next_spell='10898' WHERE spell_id = 6066;
UPDATE spell_chain SET next_spell='10526' WHERE spell_id = 8249;
UPDATE spell_chain SET next_spell='8045' WHERE spell_id = 8044;
UPDATE spell_chain SET next_spell='19276' WHERE spell_id = 2944;
UPDATE spell_chain SET next_spell='10467' WHERE spell_id = 10466;
UPDATE spell_chain SET next_spell='6392' WHERE spell_id = 6391;
UPDATE spell_chain SET next_spell='10225' WHERE spell_id = 10223;
UPDATE spell_chain SET next_spell='8052' WHERE spell_id = 8050;
UPDATE spell_chain SET next_spell='25528' WHERE spell_id = 25361;
UPDATE spell_chain SET next_spell='14320' WHERE spell_id = 14319;
UPDATE spell_chain SET next_spell='28609' WHERE spell_id = 10177;
UPDATE spell_chain SET next_spell='10947' WHERE spell_id = 10946;
UPDATE spell_chain SET next_spell='20914' WHERE spell_id = 20913;
UPDATE spell_chain SET next_spell='15264' WHERE spell_id = 15263;
UPDATE spell_chain SET next_spell='6364' WHERE spell_id = 6363;
UPDATE spell_chain SET next_spell='705' WHERE spell_id = 695;
UPDATE spell_chain SET next_spell='10412' WHERE spell_id = 8046;
UPDATE spell_chain SET next_spell='26980' WHERE spell_id = 9858;
UPDATE spell_chain SET next_spell='15629' WHERE spell_id = 14274;
UPDATE spell_chain SET next_spell='25557' WHERE spell_id = 16387;
UPDATE spell_chain SET next_spell='27226' WHERE spell_id = 11717;
UPDATE spell_chain SET next_spell='5588' WHERE spell_id = 853;
UPDATE spell_chain SET next_spell='19242' WHERE spell_id = 19241;
UPDATE spell_chain SET next_spell='9852' WHERE spell_id = 5196;
UPDATE spell_chain SET next_spell='30459' WHERE spell_id = 27210;
UPDATE spell_chain SET next_spell='25479' WHERE spell_id = 16316;
UPDATE spell_chain SET next_spell='8192' WHERE spell_id = 453;
UPDATE spell_chain SET next_spell='25295' WHERE spell_id = 13555;
UPDATE spell_chain SET next_spell='18881' WHERE spell_id = 18880;
UPDATE spell_chain SET next_spell='8402' WHERE spell_id = 8401;
UPDATE spell_chain SET next_spell='17313' WHERE spell_id = 17312;
UPDATE spell_chain SET next_spell='6060' WHERE spell_id = 1004;
UPDATE spell_chain SET next_spell='13021' WHERE spell_id = 13020;
UPDATE spell_chain SET next_spell='25585' WHERE spell_id = 10614;
UPDATE spell_chain SET next_spell='8928' WHERE spell_id = 8927;
UPDATE spell_chain SET next_spell='597' WHERE spell_id = 587;
UPDATE spell_chain SET next_spell='26969' WHERE spell_id = 25347;
UPDATE spell_chain SET next_spell='8494' WHERE spell_id = 1463;
UPDATE spell_chain SET next_spell='17727' WHERE spell_id = 2362;
UPDATE spell_chain SET next_spell='27265' WHERE spell_id = 18938;
UPDATE spell_chain SET next_spell='14298' WHERE spell_id = 13797;
UPDATE spell_chain SET next_spell='10965' WHERE spell_id = 10964;
UPDATE spell_chain SET next_spell='10941' WHERE spell_id = 9592;
UPDATE spell_chain SET next_spell='959' WHERE spell_id = 939;
UPDATE spell_chain SET next_spell='11700' WHERE spell_id = 11699;
UPDATE spell_chain SET next_spell='3421' WHERE spell_id = 3420;
UPDATE spell_chain SET next_spell='14818 27681' WHERE spell_id = 14752;
UPDATE spell_chain SET next_spell='26993' WHERE spell_id = 9907;
UPDATE spell_chain SET next_spell='20928' WHERE spell_id = 20927;
UPDATE spell_chain SET next_spell='29228' WHERE spell_id = 10448;
UPDATE spell_chain SET next_spell='6064' WHERE spell_id = 6063;
UPDATE spell_chain SET next_spell='6222' WHERE spell_id = 172;
UPDATE spell_chain SET next_spell='10876' WHERE spell_id = 10875;
UPDATE spell_chain SET next_spell='27170' WHERE spell_id = 20920;
UPDATE spell_chain SET next_spell='20292' WHERE spell_id = 20291;
UPDATE spell_chain SET next_spell='31895 31896' WHERE spell_id = 20164;
UPDATE spell_chain SET next_spell='25349' WHERE spell_id = 11354;
UPDATE spell_chain SET next_spell='26989' WHERE spell_id = 9853;
UPDATE spell_chain SET next_spell='591' WHERE spell_id = 585;
UPDATE spell_chain SET next_spell='25449' WHERE spell_id = 25448;
UPDATE spell_chain SET next_spell='8924' WHERE spell_id = 8921;
UPDATE spell_chain SET next_spell='32700' WHERE spell_id = 32699;
UPDATE spell_chain SET next_spell='5188' WHERE spell_id = 5187;
UPDATE spell_chain SET next_spell='11719' WHERE spell_id = 1714;

-- SkillLineAbilities.dbc (skips 2 that already were in the spell_chain table)
INSERT INTO `spell_chain` (`spell_id`, `prev_spell`, `next_spell`, `first_spell`, `rank`, `req_spell`) VALUES
(53, 0, '2589', 53, 1, 0),
(72, 0, '1671', 72, 1, 0),
(78, 0, '284', 78, 1, 0),
(99, 0, '1735', 99, 1, 0),
(100, 0, '6178', 100, 1, 0),
(284, 78, '285', 78, 2, 0),
(285, 284, '1608', 78, 3, 0),
(408, 0, '8643', 408, 1, 0),
(465, 0, '10290', 465, 1, 0),
(633, 0, '2800', 633, 1, 0),
(643, 10290, '10291', 465, 3, 0),
(694, 0, '7400', 694, 1, 0),
(703, 0, '8631', 703, 1, 0),
(769, 780, '9754', 779, 3, 0),
(772, 0, '6546', 772, 1, 0),
(779, 0, '780', 779, 1, 0),
(780, 779, '769', 779, 2, 0),
(845, 0, '7369', 845, 1, 0),
(1032, 10291, '10292', 465, 5, 0),
(1079, 0, '9492', 1079, 1, 0),
(1082, 0, '3029', 1082, 1, 0),
(1160, 0, '6190', 1160, 1, 0),
(1329, 0, '34411', 1329, 1, 0),
(1464, 0, '8820', 1464, 1, 0),
(1608, 285, '11564', 78, 4, 0),
(1671, 72, '1672', 72, 2, 0),
(1672, 1671, '29704', 72, 3, 0),
(1715, 0, '7372', 1715, 1, 0),
(1735, 99, '9490', 99, 2, 0),
(1742, 0, '1753', 1742, 1, 0),
(1752, 0, '1757', 1752, 1, 0),
(1753, 1742, '1754', 1742, 2, 0),
(1754, 1753, '1755', 1742, 3, 0),
(1755, 1754, '1756', 1742, 4, 0),
(1756, 1755, '16697', 1742, 5, 0),
(1757, 1752, '1758', 1752, 2, 0),
(1758, 1757, '1759', 1752, 3, 0),
(1759, 1758, '1760', 1752, 4, 0),
(1760, 1759, '8621', 1752, 5, 0),
(1766, 0, '1767', 1766, 1, 0),
(1767, 1766, '1768', 1766, 2, 0),
(1768, 1767, '1769', 1766, 3, 0),
(1769, 1768, '38768', 1766, 4, 0),
(1776, 0, '1777', 1776, 1, 0),
(1777, 1776, '8629', 1776, 2, 0),
(1784, 0, '1785', 1784, 1, 0),
(1785, 1784, '1786', 1784, 2, 0),
(1786, 1785, '1787', 1784, 3, 0),
(1787, 1786, NULL, 1784, 4, 0),
(1822, 0, '1823', 1822, 1, 0),
(1823, 1822, '1824', 1822, 2, 0),
(1824, 1823, '9904', 1822, 3, 0),
(1850, 0, '9821', 1850, 1, 0),
(1856, 0, '1857', 1856, 1, 0),
(1857, 1856, '26889', 1856, 2, 0),
(1943, 0, '8639', 1943, 1, 0),
(1966, 0, '6768', 1966, 1, 0),
(2018, 0, '3100', 2018, 1, 0),
(2048, 25289, NULL, 6673, 8, 0),
(2070, 6770, '11297', 6770, 2, 0),
(2098, 0, '6760', 2098, 1, 0),
(2108, 0, '3104', 2108, 1, 0),
(2259, 0, '3101', 2259, 1, 0),
(2366, 0, '2368', 2366, 1, 0),
(2368, 0, '3570', 2368, 1, 0),
(2383, 0, '8387', 2383, 1, 0),
(2550, 0, '3102', 2550, 1, 0),
(2575, 0, '2576', 2575, 1, 0),
(2576, 2575, '3564', 2575, 2, 0),
(2580, 0, '8388', 2580, 1, 0),
(2589, 53, '2590', 53, 2, 0),
(2590, 2589, '2591', 53, 3, 0),
(2591, 2590, '8721', 53, 4, 0),
(2649, 0, '14916', 2649, 1, 0),
(2800, 633, '10310', 633, 2, 0),
(2947, 0, '8316', 2947, 1, 0),
(2983, 0, '8696', 2983, 1, 0),
(3009, 3010, '27049', 16827, 8, 0),
(3010, 16832, '3009', 16827, 7, 0),
(3029, 1082, '5201', 1082, 2, 0),
(3100, 2018, '3538', 2018, 2, 0),
(3101, 2259, '3464', 2259, 2, 0),
(3102, 2550, '3413', 2550, 2, 0),
(3104, 2108, '3811', 2108, 2, 0),
(3110, 0, '7799', 3110, 1, 0),
(3273, 0, '3274', 3273, 1, 0),
(3274, 3273, '7924', 3273, 2, 0),
(3413, 3102, '18260', 2550, 3, 0),
(3464, 3101, '11611', 2259, 3, 0),
(3538, 3100, '9785', 2018, 3, 0),
(3564, 2576, '10248', 2575, 3, 0),
(3570, 2368, '11993', 2368, 2, 0),
(3716, 0, '7809', 3716, 1, 0),
(3811, 3104, '10662', 2108, 3, 0),
(3908, 0, '3909', 3908, 1, 0),
(3909, 3908, '3910', 3908, 2, 0),
(3910, 3909, '12180', 3908, 3, 0),
(4036, 0, '4037', 4036, 1, 0),
(4037, 4036, '4038', 4036, 2, 0),
(4038, 4037, '12656', 4036, 3, 0),
(4187, 0, '4188', 4187, 1, 0),
(4188, 4187, '4189', 4187, 2, 0),
(4189, 4188, '4190', 4187, 3, 0),
(4190, 4189, '4191', 4187, 4, 0),
(4191, 4190, '4192', 4187, 5, 0),
(4192, 4191, '4193', 4187, 6, 0),
(4193, 4192, '4194', 4187, 7, 0),
(4194, 4193, '5041', 4187, 8, 0),
(5041, 4194, '5042', 4187, 9, 0),
(5042, 5041, '27062', 4187, 10, 0),
(5171, 0, '6774', 5171, 1, 0),
(5201, 3029, '9849', 1082, 3, 0),
(5211, 0, '6798', 5211, 1, 0),
(5215, 0, '6783', 5215, 1, 0),
(5217, 0, '6793', 5217, 1, 0),
(5221, 0, '6800', 5221, 1, 0),
(5242, 6673, '6192', 6673, 2, 0),
(5277, 0, '26669', 5277, 1, 0),
(5308, 0, '20658', 5308, 1, 0),
(5487, 0, '9634', 5487, 1, 0),
(6178, 100, '11578', 100, 2, 0),
(6190, 1160, '11554', 1160, 2, 0),
(6192, 5242, '11549', 6673, 3, 0),
(6280, 0, '6281', 6280, 1, 0),
(6281, 6280, '6282', 6280, 2, 0),
(6282, 6281, '6283', 6280, 3, 0),
(6283, 6282, '6286', 6280, 4, 0),
(6286, 6283, NULL, 6280, 5, 0),
(6307, 0, '7804', 6307, 1, 0),
(6311, 0, '6314', 6311, 1, 0),
(6314, 6311, '6315', 6311, 2, 0),
(6315, 6314, '6316', 6311, 3, 0),
(6316, 6315, '6317', 6311, 4, 0),
(6317, 6316, NULL, 6311, 5, 0),
(6328, 0, '6331', 6328, 1, 0),
(6331, 6328, '6332', 6328, 2, 0),
(6332, 6331, '6333', 6328, 3, 0),
(6333, 6332, '6334', 6328, 4, 0),
(6334, 6333, NULL, 6328, 5, 0),
(6343, 0, '8198', 6343, 1, 0),
(6360, 0, '7813', 6360, 1, 0),
(6443, 0, '6444', 6443, 1, 0),
(6444, 6443, '6445', 6443, 2, 0),
(6445, 6444, '6446', 6443, 3, 0),
(6446, 6445, '6447', 6443, 4, 0),
(6447, 6446, NULL, 6443, 5, 0),
(6546, 772, '6547', 772, 2, 0),
(6547, 6546, '6548', 772, 3, 0),
(6548, 6547, '11572', 772, 4, 0),
(6552, 0, '6554', 6552, 1, 0),
(6554, 6552, NULL, 6552, 2, 0),
(6572, 0, '6574', 6572, 1, 0),
(6574, 6572, '7379', 6572, 2, 0),
(6673, 0, '5242', 6673, 1, 0),
(6760, 2098, '6761', 2098, 2, 0),
(6761, 6760, '6762', 2098, 3, 0),
(6762, 6761, '8623', 2098, 4, 0),
(6768, 1966, '8637', 1966, 2, 0),
(6770, 0, '2070', 6770, 1, 0),
(6774, 5171, NULL, 5171, 2, 0),
(6783, 5215, '9913', 5215, 2, 0),
(6785, 0, '6787', 6785, 1, 0),
(6787, 6785, '9866', 6785, 2, 0),
(6793, 5217, '9845', 5217, 2, 0),
(6798, 5211, '8983', 5211, 2, 0),
(6800, 5221, '8992', 5221, 2, 0),
(6807, 0, '6808', 6807, 1, 0),
(6808, 6807, '6809', 6807, 2, 0),
(6809, 6808, '8972', 6807, 3, 0),
(7294, 0, '10298', 7294, 1, 0),
(7369, 845, '11608', 845, 2, 0),
(7371, 0, '26177', 7371, 1, 0),
(7372, 1715, '7373', 1715, 2, 0),
(7373, 7372, '25212', 1715, 3, 0),
(7379, 6574, '11600', 6572, 3, 0),
(7384, 0, '7887', 7384, 1, 0),
(7386, 0, '7405', 7386, 1, 0),
(7400, 694, '7402', 694, 2, 0),
(7402, 7400, '20559', 694, 3, 0),
(7405, 7386, '8380', 7386, 2, 0),
(7411, 0, '7412', 7411, 1, 0),
(7412, 7411, '7413', 7411, 2, 0),
(7413, 7412, '13920', 7411, 3, 0),
(7620, 0, '7731', 7620, 1, 0),
(7731, 7620, '7732', 7620, 2, 0),
(7732, 7731, '18248', 7620, 3, 0),
(7799, 3110, '7800', 3110, 2, 0),
(7800, 7799, '7801', 3110, 3, 0),
(7801, 7800, '7802', 3110, 4, 0),
(7802, 7801, '11762', 3110, 5, 0),
(7804, 6307, '7805', 6307, 2, 0),
(7805, 7804, '11766', 6307, 3, 0),
(7809, 3716, '7810', 3716, 2, 0),
(7810, 7809, '7811', 3716, 3, 0),
(7811, 7810, '11774', 3716, 4, 0),
(7812, 0, '19438', 7812, 1, 0),
(7813, 6360, '11784', 6360, 2, 0),
(7814, 0, '7815', 7814, 1, 0),
(7815, 7814, '7816', 7814, 2, 0),
(7816, 7815, '11778', 7814, 3, 0),
(7887, 7384, '11584', 7384, 2, 0),
(7924, 3274, '10846', 3273, 3, 0),
(8198, 6343, '8204', 6343, 2, 0),
(8204, 8198, '8205', 6343, 3, 0),
(8205, 8204, '11580', 6343, 4, 0),
(8316, 2947, '8317', 2947, 2, 0),
(8317, 8316, '11770', 2947, 3, 0),
(8380, 7405, '11596', 7386, 3, 0),
(8387, 2383, NULL, 2383, 2, 0),
(8388, 2580, NULL, 2580, 2, 0),
(8613, 0, '8617', 8613, 1, 0),
(8617, 8613, '8618', 8613, 2, 0),
(8618, 8617, '10768', 8613, 3, 0),
(8621, 1760, '11293', 1752, 6, 0),
(8623, 6762, '8624', 2098, 5, 0),
(8624, 8623, '11299', 2098, 6, 0),
(8629, 1777, '11285', 1776, 3, 0),
(8631, 703, '8632', 703, 2, 0),
(8632, 8631, '8633', 703, 3, 0),
(8633, 8632, '11289', 703, 4, 0),
(8637, 6768, '11303', 1966, 3, 0),
(8639, 1943, '8640', 1943, 2, 0),
(8640, 8639, '11273', 1943, 3, 0),
(8643, 408, NULL, 408, 2, 0),
(8647, 0, '8649', 8647, 1, 0),
(8649, 8647, '8650', 8647, 2, 0),
(8650, 8649, '11197', 8647, 3, 0),
(8676, 0, '8724', 8676, 1, 0),
(8696, 2983, '11305', 2983, 2, 0),
(8721, 2591, '11279', 53, 5, 0),
(8724, 8676, '8725', 8676, 2, 0),
(8725, 8724, '11267', 8676, 3, 0),
(8820, 1464, '11604', 1464, 2, 0),
(8972, 6809, '9745', 6807, 4, 0),
(8983, 6798, NULL, 5211, 3, 0),
(8992, 6800, '9829', 5221, 3, 0),
(8998, 0, '9000', 8998, 1, 0),
(9000, 8998, '9892', 8998, 2, 0),
(9005, 0, '9823', 9005, 1, 0),
(9007, 0, '9824', 9007, 1, 0),
(9490, 1735, '9747', 99, 3, 0),
(9492, 1079, '9493', 1079, 2, 0),
(9493, 9492, '9752', 1079, 3, 0),
(9634, 5487, NULL, 5487, 2, 0),
(9745, 8972, '9880', 6807, 5, 0),
(9747, 9490, '9898', 99, 4, 0),
(9752, 9493, '9894', 1079, 4, 0),
(9754, 769, '9908', 779, 4, 0),
(9785, 3538, '29844', 2018, 4, 0),
(9821, 1850, '33357', 1850, 2, 0),
(9823, 9005, '9827', 9005, 2, 0),
(9824, 9007, '9826', 9007, 2, 0),
(9826, 9824, '27007', 9007, 3, 0),
(9827, 9823, '27006', 9005, 3, 0),
(9829, 8992, '9830', 5221, 4, 0),
(9830, 9829, '27001', 5221, 5, 0),
(9845, 6793, '9846', 5217, 3, 0),
(9846, 9845, NULL, 5217, 4, 0),
(9849, 5201, '9850', 1082, 4, 0),
(9850, 9849, '27000', 1082, 5, 0),
(9866, 6787, '9867', 6785, 3, 0),
(9867, 9866, '27005', 6785, 4, 0),
(9880, 9745, '9881', 6807, 6, 0),
(9881, 9880, '26996', 6807, 7, 0),
(9892, 9000, '31709', 8998, 3, 0),
(9894, 9752, '9896', 1079, 5, 0),
(9896, 9894, '27008', 1079, 6, 0),
(9898, 9747, '26998', 99, 5, 0),
(9904, 1824, '27003', 1822, 4, 0),
(9908, 9754, '26997', 779, 5, 0),
(9913, 6783, NULL, 5215, 3, 0),
(10248, 3564, '29354', 2575, 4, 0),
(10290, 465, '643', 465, 2, 0),
(10291, 643, '1032', 465, 4, 0),
(10292, 1032, '10293', 465, 6, 0),
(10293, 10292, '27149', 465, 7, 0),
(10298, 7294, '10299', 7294, 2, 0),
(10299, 10298, '10300', 7294, 3, 0),
(10300, 10299, '10301', 7294, 4, 0),
(10301, 10300, '27150', 7294, 5, 0),
(10310, 2800, '27154', 633, 3, 0),
(10662, 3811, '32549', 2108, 4, 0),
(10768, 8618, '32678', 8613, 4, 0),
(10846, 7924, '27028', 3273, 4, 0),
(11197, 8650, '11198', 8647, 4, 0),
(11198, 11197, '26866', 8647, 5, 0),
(11267, 8725, '11268', 8676, 4, 0),
(11268, 11267, '11269', 8676, 5, 0),
(11269, 11268, '27441', 8676, 6, 0),
(11273, 8640, '11274', 1943, 4, 0),
(11274, 11273, '11275', 1943, 5, 0),
(11275, 11274, '26867', 1943, 6, 0),
(11279, 8721, '11280', 53, 6, 0),
(11280, 11279, '11281', 53, 7, 0),
(11281, 11280, '25300', 53, 8, 0),
(11285, 8629, '11286', 1776, 4, 0),
(11286, 11285, '38764', 1776, 5, 0),
(11289, 8633, '11290', 703, 5, 0),
(11290, 11289, '26839', 703, 6, 0),
(11293, 8621, '11294', 1752, 7, 0),
(11294, 11293, '26861', 1752, 8, 0),
(11297, 2070, NULL, 6770, 3, 0),
(11299, 8624, '11300', 2098, 7, 0),
(11300, 11299, '31016', 2098, 8, 0),
(11303, 8637, '25302', 1966, 4, 0),
(11305, 8696, NULL, 2983, 3, 0),
(11549, 6192, '11550', 6673, 4, 0),
(11550, 11549, '11551', 6673, 5, 0),
(11551, 11550, '25289', 6673, 6, 0),
(11554, 6190, '11555', 1160, 3, 0),
(11555, 11554, '11556', 1160, 4, 0),
(11556, 11555, '25202', 1160, 5, 0),
(11564, 1608, '11565', 78, 5, 0),
(11565, 11564, '11566', 78, 6, 0),
(11566, 11565, '11567', 78, 7, 0),
(11567, 11566, '25286', 78, 8, 0),
(11572, 6548, '11573', 772, 5, 0),
(11573, 11572, '11574', 772, 6, 0),
(11574, 11573, '25208', 772, 7, 0),
(11578, 6178, NULL, 100, 3, 0),
(11580, 8205, '11581', 6343, 5, 0),
(11581, 11580, '25264', 6343, 6, 0),
(11584, 7887, '11585', 7384, 3, 0),
(11585, 11584, NULL, 7384, 4, 0),
(11596, 8380, '11597', 7386, 4, 0),
(11597, 11596, '25225', 7386, 5, 0),
(11600, 7379, '11601', 6572, 4, 0),
(11601, 11600, '25288', 6572, 5, 0),
(11604, 8820, '11605', 1464, 3, 0),
(11605, 11604, '25241', 1464, 4, 0),
(11608, 7369, '11609', 845, 3, 0),
(11609, 11608, '20569', 845, 4, 0),
(11611, 3464, '28596', 2259, 4, 0),
(11762, 7802, '11763', 3110, 6, 0),
(11763, 11762, '27267', 3110, 7, 0),
(11766, 7805, '11767', 6307, 4, 0),
(11767, 11766, '27268', 6307, 5, 0),
(11770, 8317, '11771', 2947, 4, 0),
(11771, 11770, '27269', 2947, 5, 0),
(11774, 7811, '11775', 3716, 5, 0),
(11775, 11774, '27270', 3716, 6, 0),
(11778, 7816, '11779', 7814, 4, 0),
(11779, 11778, '11780', 7814, 5, 0),
(11780, 11779, '27274', 7814, 6, 0),
(11784, 7813, '11785', 6360, 3, 0),
(11785, 11784, '27275', 6360, 4, 0),
(11993, 3570, '28695', 2368, 3, 0),
(12180, 3910, '26790', 3908, 4, 0),
(12294, 0, '21551', 12294, 1, 0),
(12656, 4038, '30350', 4036, 4, 0),
(13920, 7413, '28029', 7411, 4, 0),
(14916, 2649, '14917', 2649, 2, 0),
(14917, 14916, '14918', 2649, 3, 0),
(14918, 14917, '14919', 2649, 4, 0),
(14919, 14918, '14920', 2649, 5, 0),
(14920, 14919, '14921', 2649, 6, 0),
(14921, 14920, '27047', 2649, 7, 0),
(16511, 0, '17347', 16511, 1, 0),
(16697, 1756, '27048', 1742, 6, 0),
(16827, 0, '16828', 16827, 1, 0),
(16828, 16827, '16829', 16827, 2, 0),
(16829, 16828, '16830', 16827, 3, 0),
(16830, 16829, '16831', 16827, 4, 0),
(16831, 16830, '16832', 16827, 5, 0),
(16832, 16831, '3010', 16827, 6, 0),
(16952, 0, '16954', 16952, 1, 0),
(16954, 16952, NULL, 16952, 2, 0),
(16958, 0, '16961', 16958, 1, 0),
(16961, 16958, NULL, 16958, 2, 0),
(17253, 0, '17255', 17253, 1, 0),
(17255, 17253, '17256', 17253, 2, 0),
(17256, 17255, '17257', 17253, 3, 0),
(17257, 17256, '17258', 17253, 4, 0),
(17258, 17257, '17259', 17253, 5, 0),
(17259, 17258, '17260', 17253, 6, 0),
(17260, 17259, '17261', 17253, 7, 0),
(17261, 17260, '27050', 17253, 8, 0),
(17347, 16511, '17348', 16511, 2, 0),
(17348, 17347, '26864', 16511, 3, 0),
(17735, 0, '17750', 17735, 1, 0),
(17750, 17735, '17751', 17735, 2, 0),
(17751, 17750, '17752', 17735, 3, 0),
(17752, 17751, '27271', 17735, 4, 0),
(17767, 0, '17850', 17767, 1, 0),
(17850, 17767, '17851', 17767, 2, 0),
(17851, 17850, '17852', 17767, 3, 0),
(17852, 17851, '17853', 17767, 4, 0),
(17853, 17852, '17854', 17767, 5, 0),
(17854, 17853, '27272', 17767, 6, 0),
(18248, 7732, '33095', 7620, 4, 0),
(18260, 3413, '33359', 2550, 4, 0),
(19244, 0, '19647', 19244, 1, 0),
(19438, 7812, '19440', 7812, 2, 0),
(19440, 19438, '19441', 7812, 3, 0),
(19441, 19440, '19442', 7812, 4, 0),
(19442, 19441, '19443', 7812, 5, 0),
(19443, 19442, '27273', 7812, 6, 0),
(19478, 0, '19655', 19478, 1, 0),
(19505, 0, '19731', 19505, 1, 0),
(19506, 0, '20905', 19506, 1, 0),
(19647, 19244, NULL, 19244, 2, 0),
(19655, 19478, '19656', 19478, 2, 0),
(19656, 19655, '19660', 19478, 3, 0),
(19660, 19656, '27280', 19478, 4, 0),
(19731, 19505, '19734', 19505, 2, 0),
(19734, 19731, '19736', 19505, 3, 0),
(19735, 0, '27278', 19735, 1, 0),
(19736, 19734, '27276', 19505, 4, 0),
(19876, 0, '19895', 19876, 1, 0),
(19888, 0, '19897', 19888, 1, 0),
(19891, 0, '19899', 19891, 1, 0),
(19895, 19876, '19896', 19876, 2, 0),
(19896, 19895, '27151', 19876, 3, 0),
(19897, 19888, '19898', 19888, 2, 0),
(19898, 19897, '27152', 19888, 3, 0),
(19899, 19891, '19900', 19891, 2, 0),
(19900, 19899, '27153', 19891, 3, 0),
(20154, 0, '21084', 20154, 1, 0),
(20243, 0, '30016', 20243, 1, 0),
(20252, 0, '20616', 20252, 1, 0),
(20559, 7402, '20560', 694, 4, 0),
(20560, 20559, '25266', 694, 5, 0),
(20569, 11609, '25231', 845, 5, 0),
(20616, 20252, '20617', 20252, 2, 0),
(20617, 20616, '25272', 20252, 3, 0),
(20658, 5308, '20660', 5308, 2, 0),
(20660, 20658, '20661', 5308, 3, 0),
(20661, 20660, '20662', 5308, 4, 0),
(20662, 20661, '25234', 5308, 5, 0),
(20905, 19506, '20906', 19506, 2, 0),
(20906, 20905, '27066', 19506, 3, 0),
(21084, 20154, NULL, 20154, 2, 0),
(21551, 12294, '21552', 12294, 2, 0),
(21552, 21551, '21553', 12294, 3, 0),
(21553, 21552, '25248', 12294, 4, 0),
(22568, 0, '22827', 22568, 1, 0),
(22827, 22568, '22828', 22568, 2, 0),
(22828, 22827, '22829', 22568, 3, 0),
(22829, 22828, '31018', 22568, 4, 0),
(22842, 0, '22895', 22842, 1, 0),
(22895, 22842, '22896', 22842, 2, 0),
(22896, 22895, '26999', 22842, 3, 0),
(23099, 0, '23109', 23099, 1, 0),
(23109, 23099, '23110', 23099, 2, 0),
(23110, 23109, NULL, 23099, 3, 0),
(23145, 0, '23147', 23145, 1, 0),
(23147, 23145, '23148', 23145, 2, 0),
(23148, 23147, NULL, 23145, 3, 0),
(23881, 0, '23892', 23881, 1, 0),
(23892, 23881, '23893', 23881, 2, 0),
(23893, 23892, '23894', 23881, 3, 0),
(23894, 23893, '25251', 23881, 4, 0),
(23922, 0, '23923', 23922, 1, 0),
(23923, 23922, '23924', 23922, 2, 0),
(23924, 23923, '23925', 23922, 3, 0),
(23925, 23924, '25258', 23922, 4, 0),
(23992, 0, '24439', 23992, 1, 0),
(24248, 31018, NULL, 22568, 6, 0),
(24423, 0, '24577', 24423, 1, 0),
(24439, 23992, '24444', 23992, 2, 0),
(24444, 24439, '24445', 23992, 3, 0),
(24445, 24444, '27053', 23992, 4, 0),
(24446, 0, '24447', 24446, 1, 0),
(24447, 24446, '24448', 24446, 2, 0),
(24448, 24447, '24449', 24446, 3, 0),
(24449, 24448, '27054', 24446, 4, 0),
(24450, 0, '24452', 24450, 1, 0),
(24452, 24450, '24453', 24450, 2, 0),
(24453, 24452, NULL, 24450, 3, 0),
(24488, 0, '24505', 24488, 1, 0),
(24492, 0, '24502', 24492, 1, 0),
(24493, 0, '24497', 24493, 1, 0),
(24497, 24493, '24500', 24493, 2, 0),
(24500, 24497, '24501', 24493, 3, 0),
(24501, 24500, '27052', 24493, 4, 0),
(24502, 24492, '24503', 24492, 2, 0),
(24503, 24502, '24504', 24492, 3, 0),
(24504, 24503, '27055', 24492, 4, 0),
(24505, 24488, '24506', 24488, 2, 0),
(24506, 24505, '24507', 24488, 3, 0),
(24507, 24506, '27056', 24488, 4, 0),
(24545, 0, '24549', 24545, 1, 0),
(24549, 24545, '24550', 24545, 2, 0),
(24550, 24549, '24551', 24545, 3, 0),
(24551, 24550, '24552', 24545, 4, 0),
(24552, 24551, '24553', 24545, 5, 0),
(24553, 24552, '24554', 24545, 6, 0),
(24554, 24553, '24555', 24545, 7, 0),
(24555, 24554, '24629', 24545, 8, 0),
(24577, 24423, '24578', 24423, 2, 0),
(24578, 24577, '24579', 24423, 3, 0),
(24579, 24578, '27051', 24423, 4, 0),
(24583, 24640, '24586', 24640, 2, 0),
(24586, 24583, '24587', 24640, 3, 0),
(24587, 24586, '27060', 24640, 4, 0),
(24597, 24603, NULL, 24604, 4, 0),
(24603, 24605, '24597', 24604, 3, 0),
(24604, 0, '24605', 24604, 1, 0),
(24605, 24604, '24603', 24604, 2, 0),
(24629, 24555, '24630', 24545, 9, 0),
(24630, 24629, '27061', 24545, 10, 0),
(24640, 0, '24583', 24640, 1, 0),
(24844, 0, '25008', 24844, 1, 0),
(25008, 24844, '25009', 24844, 2, 0),
(25009, 25008, '25010', 24844, 3, 0),
(25010, 25009, '25011', 24844, 4, 0),
(25011, 25010, '25012', 24844, 5, 0),
(25012, 25011, NULL, 24844, 6, 0),
(25202, 11556, '25203', 1160, 6, 0),
(25203, 25202, NULL, 1160, 7, 0),
(25208, 11574, NULL, 772, 8, 0),
(25212, 7373, NULL, 1715, 4, 0),
(25225, 11597, NULL, 7386, 6, 0),
(25229, 0, '25230', 25229, 1, 0),
(25230, 25229, '28894', 25229, 2, 0),
(25231, 20569, NULL, 845, 6, 0),
(25234, 20662, '25236', 5308, 6, 0),
(25236, 25234, NULL, 5308, 7, 0),
(25241, 11605, '25242', 1464, 5, 0),
(25242, 25241, NULL, 1464, 6, 0),
(25248, 21553, '30330', 12294, 5, 0),
(25251, 23894, '30335', 23881, 5, 0),
(25258, 23925, '30356', 23922, 5, 0),
(25264, 11581, NULL, 6343, 7, 0),
(25266, 20560, NULL, 694, 6, 0),
(25269, 25288, '30357', 6572, 7, 0),
(25272, 20617, '25275', 20252, 4, 0),
(25275, 25272, NULL, 20252, 5, 0),
(25286, 11567, '29707', 78, 9, 0),
(25288, 11601, '25269', 6572, 6, 0),
(25289, 11551, '2048', 6673, 7, 0),
(25300, 11281, '26863', 53, 9, 0),
(25302, 11303, '27448', 1966, 5, 0),
(26090, 0, '26187', 26090, 1, 0),
(26177, 7371, '26178', 7371, 2, 0),
(26178, 26177, '26179', 7371, 3, 0),
(26179, 26178, '26201', 7371, 4, 0),
(26187, 26090, '26188', 26090, 2, 0),
(26188, 26187, '27063', 26090, 3, 0),
(26201, 26179, '27685', 7371, 5, 0),
(26669, 5277, NULL, 5277, 2, 0),
(26790, 12180, NULL, 3908, 5, 0),
(26839, 11290, '26884', 703, 7, 0),
(26861, 11294, '26862', 1752, 9, 0),
(26862, 26861, NULL, 1752, 10, 0),
(26863, 25300, NULL, 53, 10, 0),
(26864, 17348, NULL, 16511, 4, 0),
(26865, 31016, NULL, 2098, 10, 0),
(26866, 11198, NULL, 8647, 6, 0),
(26867, 11275, NULL, 1943, 7, 0),
(26884, 26839, NULL, 703, 8, 0),
(26889, 1857, NULL, 1856, 3, 0),
(26996, 9881, NULL, 6807, 8, 0),
(26997, 9908, NULL, 779, 6, 0),
(26998, 9898, NULL, 99, 6, 0),
(26999, 22896, NULL, 22842, 4, 0),
(27000, 9850, NULL, 1082, 6, 0),
(27001, 9830, '27002', 5221, 6, 0),
(27002, 27001, NULL, 5221, 7, 0),
(27003, 9904, NULL, 1822, 5, 0),
(27004, 31709, NULL, 8998, 5, 0),
(27005, 9867, NULL, 6785, 5, 0),
(27006, 9827, NULL, 9005, 4, 0),
(27007, 9826, NULL, 9007, 4, 0),
(27008, 9896, NULL, 1079, 7, 0),
(27028, 10846, NULL, 3273, 5, 0),
(27047, 14921, NULL, 2649, 8, 0),
(27048, 16697, NULL, 1742, 7, 0),
(27049, 3009, NULL, 16827, 9, 0),
(27050, 17261, NULL, 17253, 9, 0),
(27051, 24579, NULL, 24423, 5, 0),
(27052, 24501, NULL, 24493, 5, 0),
(27053, 24445, NULL, 23992, 5, 0),
(27054, 24449, NULL, 24446, 5, 0),
(27055, 24504, NULL, 24492, 5, 0),
(27056, 24507, NULL, 24488, 5, 0),
(27060, 24587, NULL, 24640, 5, 0),
(27061, 24630, NULL, 24545, 11, 0),
(27062, 5042, NULL, 4187, 11, 0),
(27063, 26188, NULL, 26090, 4, 0),
(27066, 20906, NULL, 19506, 4, 0),
(27149, 10293, NULL, 465, 8, 0),
(27150, 10301, NULL, 7294, 6, 0),
(27151, 19896, NULL, 19876, 4, 0),
(27152, 19898, NULL, 19888, 4, 0),
(27153, 19900, NULL, 19891, 4, 0),
(27154, 10310, NULL, 633, 4, 0),
(27267, 11763, NULL, 3110, 8, 0),
(27268, 11767, NULL, 6307, 6, 0),
(27269, 11771, NULL, 2947, 6, 0),
(27270, 11775, NULL, 3716, 7, 0),
(27271, 17752, '33701', 17735, 5, 0),
(27272, 17854, NULL, 17767, 7, 0),
(27273, 19443, NULL, 7812, 7, 0),
(27274, 11780, NULL, 7814, 7, 0),
(27275, 11785, NULL, 6360, 5, 0),
(27276, 19736, '27277', 19505, 5, 0),
(27277, 27276, NULL, 19505, 6, 0),
(27278, 19735, '27279', 19735, 2, 0),
(27279, 27278, NULL, 19735, 3, 0),
(27280, 19660, NULL, 19478, 5, 0),
(27441, 11269, NULL, 8676, 7, 0),
(27448, 25302, NULL, 1966, 6, 0),
(27685, 26201, NULL, 7371, 6, 0),
(28029, 13920, NULL, 7411, 5, 0),
(28596, 11611, NULL, 2259, 5, 0),
(28695, 11993, NULL, 2368, 4, 0),
(28894, 25230, '28895', 25229, 3, 0),
(28895, 28894, '28897', 25229, 4, 0),
(28897, 28895, NULL, 25229, 5, 0),
(29354, 10248, NULL, 2575, 5, 0),
(29704, 1672, NULL, 72, 4, 0),
(29707, 25286, '30324', 78, 10, 0),
(29801, 0, '30030', 29801, 1, 0),
(29844, 9785, NULL, 2018, 5, 0),
(30016, 20243, '30022', 20243, 2, 0),
(30022, 30016, NULL, 20243, 3, 0),
(30030, 29801, '30033', 29801, 2, 0),
(30033, 30030, NULL, 29801, 3, 0),
(30151, 0, '30194', 30151, 1, 0),
(30194, 30151, '30198', 30151, 2, 0),
(30198, 30194, NULL, 30151, 3, 0),
(30213, 0, '30219', 30213, 1, 0),
(30219, 30213, '30223', 30213, 2, 0),
(30223, 30219, NULL, 30213, 3, 0),
(30324, 29707, NULL, 78, 11, 0),
(30330, 25248, NULL, 12294, 6, 0),
(30335, 25251, NULL, 23881, 6, 0),
(30350, 12656, NULL, 4036, 5, 0),
(30356, 25258, NULL, 23922, 6, 0),
(30357, 25269, NULL, 6572, 8, 0),
(31016, 11300, '26865', 2098, 9, 0),
(31018, 22829, '24248', 22568, 5, 0),
(31709, 9892, '27004', 8998, 4, 0),
(31785, 0, '33776', 31785, 1, 0),
(32549, 10662, NULL, 2108, 5, 0),
(32645, 0, '32684', 32645, 1, 0),
(32678, 10768, NULL, 8613, 5, 0),
(32684, 32645, NULL, 32645, 2, 0),
(33095, 18248, NULL, 7620, 5, 0),
(33357, 9821, NULL, 1850, 3, 0),
(33359, 18260, NULL, 2550, 5, 0),
(33388, 0, '33391', 33388, 1, 0),
(33391, 33388, '34090', 33388, 2, 0),
(33698, 0, '33699', 33698, 1, 0),
(33699, 33698, '33700', 33698, 2, 0),
(33700, 33699, NULL, 33698, 3, 0),
(33701, 27271, NULL, 17735, 6, 0),
(33776, 31785, NULL, 31785, 2, 0),
(33876, 0, '33982', 33876, 1, 0),
(33878, 0, '33986', 33878, 1, 0),
(33943, 0, '40120', 33943, 1, 0),
(33982, 33876, '33983', 33876, 2, 0),
(33983, 33982, NULL, 33876, 3, 0),
(33986, 33878, '33987', 33878, 2, 0),
(33987, 33986, NULL, 33878, 3, 0),
(34090, 33391, '34091', 33388, 3, 0),
(34091, 34090, NULL, 33388, 4, 0),
(34411, 1329, '34412', 1329, 2, 0),
(34412, 34411, '34413', 1329, 3, 0),
(34413, 34412, NULL, 1329, 4, 0),
(34889, 0, '35323', 34889, 1, 0),
(35290, 0, '35291', 35290, 1, 0),
(35291, 35290, '35292', 35290, 2, 0),
(35292, 35291, '35293', 35290, 3, 0),
(35293, 35292, '35294', 35290, 4, 0),
(35294, 35293, '35295', 35290, 5, 0),
(35295, 35294, '35296', 35290, 6, 0),
(35296, 35295, '35297', 35290, 7, 0),
(35297, 35296, '35298', 35290, 8, 0),
(35298, 35297, NULL, 35290, 9, 0),
(35323, 34889, NULL, 34889, 2, 0),
(35387, 0, '35389', 35387, 1, 0),
(35389, 35387, '35392', 35387, 2, 0),
(35392, 35389, NULL, 35387, 3, 0),
(35694, 0, '35698', 35694, 1, 0),
(35698, 35694, NULL, 35694, 2, 0),
(38764, 11286, NULL, 1776, 6, 0),
(38768, 1769, NULL, 1766, 5, 0),
(40120, 33943, NULL, 33943, 2, 0);

-- Adds all talents that has more than one rank
INSERT INTO `spell_chain` (`spell_id`, `prev_spell`, `next_spell`, `first_spell`, `rank`, `req_spell`) VALUES
(11083, 0, '12351', 11083, 1, 0),
(12351, 11083, NULL, 11083, 2, 0),
(11094, 0, '13043', 11094, 1, 0),
(13043, 11094, NULL, 11094, 2, 0),
(11095, 0, '12872', 11095, 1, 0),
(12872, 11095, '12873', 11095, 2, 0),
(12873, 12872, NULL, 11095, 3, 0),
(11069, 0, '12338', 11069, 1, 0),
(12338, 11069, '12339', 11069, 2, 0),
(12339, 12338, '12340', 11069, 3, 0),
(12340, 12339, '12341', 11069, 4, 0),
(12341, 12340, NULL, 11069, 5, 0),
(11078, 0, '11080', 11078, 1, 0),
(11080, 11078, '12342', 11078, 2, 0),
(12342, 11080, NULL, 11078, 3, 0),
(11100, 0, '12353', 11100, 1, 0),
(12353, 11100, NULL, 11100, 2, 0),
(11103, 0, '12357', 11103, 1, 0),
(12357, 11103, '12358', 11103, 2, 0),
(12358, 12357, '12359', 11103, 3, 0),
(12359, 12358, '12360', 11103, 4, 0),
(12360, 12359, NULL, 11103, 5, 0),
(11108, 0, '12349', 11108, 1, 0),
(12349, 11108, '12350', 11108, 2, 0),
(12350, 12349, NULL, 11108, 3, 0),
(11115, 0, '11367', 11115, 1, 0),
(11367, 11115, '11368', 11115, 2, 0),
(11368, 11367, NULL, 11115, 3, 0),
(11119, 0, '11120', 11119, 1, 0),
(11120, 11119, '12846', 11119, 2, 0),
(12846, 11120, '12847', 11119, 3, 0),
(12847, 12846, '12848', 11119, 4, 0),
(12848, 12847, NULL, 11119, 5, 0),
(11124, 0, '12378', 11124, 1, 0),
(12378, 11124, '12398', 11124, 2, 0),
(12398, 12378, '12399', 11124, 3, 0),
(12399, 12398, '12400', 11124, 4, 0),
(12400, 12399, NULL, 11124, 5, 0),
(11070, 0, '12473', 11070, 1, 0),
(12473, 11070, '16763', 11070, 2, 0),
(16763, 12473, '16765', 11070, 3, 0),
(16765, 16763, '16766', 11070, 4, 0),
(16766, 16765, NULL, 11070, 5, 0),
(11071, 0, '12496', 11071, 1, 0),
(12496, 11071, '12497', 11071, 2, 0),
(12497, 12496, NULL, 11071, 3, 0),
(11151, 0, '12952', 11151, 1, 0),
(12952, 11151, '12953', 11151, 2, 0),
(12953, 12952, NULL, 11151, 3, 0),
(11165, 0, '12475', 11165, 1, 0),
(12475, 11165, NULL, 11165, 2, 0),
(11185, 0, '12487', 11185, 1, 0),
(12487, 11185, '12488', 11185, 2, 0),
(12488, 12487, NULL, 11185, 3, 0),
(11190, 0, '12489', 11190, 1, 0),
(12489, 11190, '12490', 11190, 2, 0),
(12490, 12489, NULL, 11190, 3, 0),
(11175, 0, '12569', 11175, 1, 0),
(12569, 11175, '12571', 11175, 2, 0),
(12571, 12569, NULL, 11175, 3, 0),
(11160, 0, '12518', 11160, 1, 0),
(12518, 11160, '12519', 11160, 2, 0),
(12519, 12518, NULL, 11160, 3, 0),
(11170, 0, '12982', 11170, 1, 0),
(12982, 11170, '12983', 11170, 2, 0),
(12983, 12982, '12984', 11170, 3, 0),
(12984, 12983, '12985', 11170, 4, 0),
(12985, 12984, NULL, 11170, 5, 0),
(11180, 0, '28592', 11180, 1, 0),
(28592, 11180, '28593', 11180, 2, 0),
(28593, 28592, '28594', 11180, 3, 0),
(28594, 28593, '28595', 11180, 4, 0),
(28595, 28594, NULL, 11180, 5, 0),
(11189, 0, '28332', 11189, 1, 0),
(28332, 11189, NULL, 11189, 2, 0),
(11207, 0, '12672', 11207, 1, 0),
(12672, 11207, '15047', 11207, 2, 0),
(15047, 12672, '15052', 11207, 3, 0),
(15052, 15047, '15053', 11207, 4, 0),
(15053, 15052, NULL, 11207, 5, 0),
(11210, 0, '12592', 11210, 1, 0),
(12592, 11210, NULL, 11210, 2, 0),
(11213, 0, '12574', 11213, 1, 0),
(12574, 11213, '12575', 11213, 2, 0),
(12575, 12574, '12576', 11213, 3, 0),
(12576, 12575, '12577', 11213, 4, 0),
(12577, 12576, NULL, 11213, 5, 0),
(11222, 0, '12839', 11222, 1, 0),
(12839, 11222, '12840', 11222, 2, 0),
(12840, 12839, '12841', 11222, 3, 0),
(12841, 12840, '12842', 11222, 4, 0),
(12842, 12841, NULL, 11222, 5, 0),
(11232, 0, '12500', 11232, 1, 0),
(12500, 11232, '12501', 11232, 2, 0),
(12501, 12500, '12502', 11232, 3, 0),
(12502, 12501, '12503', 11232, 4, 0),
(12503, 12502, NULL, 11232, 5, 0),
(6057, 0, '6085', 6057, 1, 0),
(6085, 6057, NULL, 6057, 2, 0),
(11237, 0, '12463', 11237, 1, 0),
(12463, 11237, '12464', 11237, 2, 0),
(12464, 12463, '16769', 11237, 3, 0),
(16769, 12464, '16770', 11237, 4, 0),
(16770, 16769, NULL, 11237, 5, 0),
(11242, 0, '12467', 11242, 1, 0),
(12467, 11242, '12469', 11242, 2, 0),
(12469, 12467, NULL, 11242, 3, 0),
(11247, 0, '12606', 11247, 1, 0),
(12606, 11247, NULL, 11247, 2, 0),
(11252, 0, '12605', 11252, 1, 0),
(12605, 11252, NULL, 11252, 2, 0),
(11255, 0, '12598', 11255, 1, 0),
(12598, 11255, NULL, 11255, 2, 0),
(12834, 0, '12849', 12834, 1, 0),
(12849, 12834, '12867', 12834, 2, 0),
(12867, 12849, NULL, 12834, 3, 0),
(12281, 0, '12812', 12281, 1, 0),
(12812, 12281, '12813', 12281, 2, 0),
(12813, 12812, '12814', 12281, 3, 0),
(12814, 12813, '12815', 12281, 4, 0),
(12815, 12814, NULL, 12281, 5, 0),
(12282, 0, '12663', 12282, 1, 0),
(12663, 12282, '12664', 12282, 2, 0),
(12664, 12663, NULL, 12282, 3, 0),
(12284, 0, '12701', 12284, 1, 0),
(12701, 12284, '12702', 12284, 2, 0),
(12702, 12701, '12703', 12284, 3, 0),
(12703, 12702, '12704', 12284, 4, 0),
(12704, 12703, NULL, 12284, 5, 0),
(12285, 0, '12697', 12285, 1, 0),
(12697, 12285, NULL, 12285, 2, 0),
(12286, 0, '12658', 12286, 1, 0),
(12658, 12286, '12659', 12286, 2, 0),
(12659, 12658, NULL, 12286, 3, 0),
(12287, 0, '12665', 12287, 1, 0),
(12665, 12287, '12666', 12287, 2, 0),
(12666, 12665, NULL, 12287, 3, 0),
(12289, 0, '12668', 12289, 1, 0),
(12668, 12289, '23695', 12289, 2, 0),
(23695, 12668, NULL, 12289, 3, 0),
(16462, 0, '16463', 16462, 1, 0),
(16463, 16462, '16464', 16462, 2, 0),
(16464, 16463, '16465', 16462, 3, 0),
(16465, 16464, '16466', 16462, 4, 0),
(16466, 16465, NULL, 16462, 5, 0),
(12290, 0, '12963', 12290, 1, 0),
(12963, 12290, NULL, 12290, 2, 0),
(12700, 0, '12781', 12700, 1, 0),
(12781, 12700, '12783', 12700, 2, 0),
(12783, 12781, '12784', 12700, 3, 0),
(12784, 12783, '12785', 12700, 4, 0),
(12785, 12784, NULL, 12700, 5, 0),
(29888, 0, '29889', 29888, 1, 0),
(29889, 29888, NULL, 29888, 2, 0),
(12163, 0, '12711', 12163, 1, 0),
(12711, 12163, '12712', 12163, 2, 0),
(12712, 12711, '12713', 12163, 3, 0),
(12713, 12712, '12714', 12163, 4, 0),
(12714, 12713, NULL, 12163, 5, 0),
(12297, 0, '12750', 12297, 1, 0),
(12750, 12297, '12751', 12297, 2, 0),
(12751, 12750, '12752', 12297, 3, 0),
(12752, 12751, '12753', 12297, 4, 0),
(12753, 12752, NULL, 12297, 5, 0),
(12299, 0, '12761', 12299, 1, 0),
(12761, 12299, '12762', 12299, 2, 0),
(12762, 12761, '12763', 12299, 3, 0),
(12763, 12762, '12764', 12299, 4, 0),
(12764, 12763, NULL, 12299, 5, 0),
(12295, 0, '12676', 12295, 1, 0),
(12676, 12295, '12677', 12295, 2, 0),
(12677, 12676, NULL, 12295, 3, 0),
(12301, 0, '12818', 12301, 1, 0),
(12818, 12301, NULL, 12301, 2, 0),
(12302, 0, '12765', 12302, 1, 0),
(12765, 12302, NULL, 12302, 2, 0),
(12303, 0, '12788', 12303, 1, 0),
(12788, 12303, '12789', 12303, 2, 0),
(12789, 12788, NULL, 12303, 3, 0),
(12308, 0, '12810', 12308, 1, 0),
(12810, 12308, '12811', 12308, 2, 0),
(12811, 12810, NULL, 12308, 3, 0),
(12797, 0, '12799', 12797, 1, 0),
(12799, 12797, '12800', 12797, 2, 0),
(12800, 12799, NULL, 12797, 3, 0),
(12311, 0, '12958', 12311, 1, 0),
(12958, 12311, NULL, 12311, 2, 0),
(12312, 0, '12803', 12312, 1, 0),
(12803, 12312, NULL, 12312, 2, 0),
(12313, 0, '12804', 12313, 1, 0),
(12804, 12313, '12807', 12313, 2, 0),
(12807, 12804, NULL, 12313, 3, 0),
(12318, 0, '12857', 12318, 1, 0),
(12857, 12318, '12858', 12318, 2, 0),
(12858, 12857, '12860', 12318, 3, 0),
(12860, 12858, '12861', 12318, 4, 0),
(12861, 12860, NULL, 12318, 5, 0),
(12317, 0, '13045', 12317, 1, 0),
(13045, 12317, '13046', 12317, 2, 0),
(13046, 13045, '13047', 12317, 3, 0),
(13047, 13046, '13048', 12317, 4, 0),
(13048, 13047, NULL, 12317, 5, 0),
(12319, 0, '12971', 12319, 1, 0),
(12971, 12319, '12972', 12319, 2, 0),
(12972, 12971, '12973', 12319, 3, 0),
(12973, 12972, '12974', 12319, 4, 0),
(12974, 12973, NULL, 12319, 5, 0),
(12320, 0, '12852', 12320, 1, 0),
(12852, 12320, '12853', 12320, 2, 0),
(12853, 12852, '12855', 12320, 3, 0),
(12855, 12853, '12856', 12320, 4, 0),
(12856, 12855, NULL, 12320, 5, 0),
(12321, 0, '12835', 12321, 1, 0),
(12835, 12321, '12836', 12321, 2, 0),
(12836, 12835, '12837', 12321, 3, 0),
(12837, 12836, '12838', 12321, 4, 0),
(12838, 12837, NULL, 12321, 5, 0),
(12322, 0, '12999', 12322, 1, 0),
(12999, 12322, '13000', 12322, 2, 0),
(13000, 12999, '13001', 12322, 3, 0),
(13001, 13000, '13002', 12322, 4, 0),
(13002, 13001, NULL, 12322, 5, 0),
(12324, 0, '12876', 12324, 1, 0),
(12876, 12324, '12877', 12324, 2, 0),
(12877, 12876, '12878', 12324, 3, 0),
(12878, 12877, '12879', 12324, 4, 0),
(12879, 12878, NULL, 12324, 5, 0),
(12329, 0, '12950', 12329, 1, 0),
(12950, 12329, '20496', 12329, 2, 0),
(20496, 12950, NULL, 12329, 3, 0),
(12862, 0, '12330', 12862, 1, 0),
(12330, 12862, NULL, 12862, 2, 0),
(13705, 0, '13832', 13705, 1, 0),
(13832, 13705, '13843', 13705, 2, 0),
(13843, 13832, '13844', 13705, 3, 0),
(13844, 13843, '13845', 13705, 4, 0),
(13845, 13844, NULL, 13705, 5, 0),
(13706, 0, '13804', 13706, 1, 0),
(13804, 13706, '13805', 13706, 2, 0),
(13805, 13804, '13806', 13706, 3, 0),
(13806, 13805, '13807', 13706, 4, 0),
(13807, 13806, NULL, 13706, 5, 0),
(13707, 0, '13966', 13707, 1, 0),
(13966, 13707, '13967', 13707, 2, 0),
(13967, 13966, '13968', 13707, 3, 0),
(13968, 13967, '13969', 13707, 4, 0),
(13969, 13968, NULL, 13707, 5, 0),
(13709, 0, '13800', 13709, 1, 0),
(13800, 13709, '13801', 13709, 2, 0),
(13801, 13800, '13802', 13709, 3, 0),
(13802, 13801, '13803', 13709, 4, 0),
(13803, 13802, NULL, 13709, 5, 0),
(13712, 0, '13788', 13712, 1, 0),
(13788, 13712, '13789', 13712, 2, 0),
(13789, 13788, '13790', 13712, 3, 0),
(13790, 13789, '13791', 13712, 4, 0),
(13791, 13790, NULL, 13712, 5, 0),
(13713, 0, '13853', 13713, 1, 0),
(13853, 13713, '13854', 13713, 2, 0),
(13854, 13853, '13855', 13713, 3, 0),
(13855, 13854, '13856', 13713, 4, 0),
(13856, 13855, NULL, 13713, 5, 0),
(13732, 0, '13863', 13732, 1, 0),
(13863, 13732, NULL, 13732, 2, 0),
(13741, 0, '13793', 13741, 1, 0),
(13793, 13741, '13792', 13741, 2, 0),
(13792, 13793, NULL, 13741, 3, 0),
(13742, 0, '13872', 13742, 1, 0),
(13872, 13742, NULL, 13742, 2, 0),
(13754, 0, '13867', 13754, 1, 0),
(13867, 13754, NULL, 13754, 2, 0),
(13715, 0, '13848', 13715, 1, 0),
(13848, 13715, '13849', 13715, 2, 0),
(13849, 13848, '13851', 13715, 3, 0),
(13851, 13849, '13852', 13715, 4, 0),
(13852, 13851, NULL, 13715, 5, 0),
(13743, 0, '13875', 13743, 1, 0),
(13875, 13743, NULL, 13743, 2, 0),
(13958, 0, '13970', 13958, 1, 0),
(13970, 13958, '13971', 13958, 2, 0),
(13971, 13970, '13972', 13958, 3, 0),
(13972, 13971, '13973', 13958, 4, 0),
(13973, 13972, NULL, 13958, 5, 0),
(13960, 0, '13961', 13960, 1, 0),
(13961, 13960, '13962', 13960, 2, 0),
(13962, 13961, '13963', 13960, 3, 0),
(13963, 13962, '13964', 13960, 4, 0),
(13964, 13963, NULL, 13960, 5, 0),
(13975, 0, '14062', 13975, 1, 0),
(14062, 13975, '14063', 13975, 2, 0),
(14063, 14062, '14064', 13975, 3, 0),
(14064, 14063, '14065', 13975, 4, 0),
(14065, 14064, NULL, 13975, 5, 0),
(13976, 0, '13979', 13976, 1, 0),
(13979, 13976, '13980', 13976, 2, 0),
(13980, 13979, NULL, 13976, 3, 0),
(13983, 0, '14070', 13983, 1, 0),
(14070, 13983, '14071', 13983, 2, 0),
(14071, 14070, NULL, 13983, 3, 0),
(13981, 0, '14066', 13981, 1, 0),
(14066, 13981, NULL, 13981, 2, 0),
(14057, 0, '14072', 14057, 1, 0),
(14072, 14057, '14073', 14057, 2, 0),
(14073, 14072, '14074', 14057, 3, 0),
(14074, 14073, '14075', 14057, 4, 0),
(14075, 14074, NULL, 14057, 5, 0),
(14076, 0, '14094', 14076, 1, 0),
(14094, 14076, NULL, 14076, 2, 0),
(14079, 0, '14080', 14079, 1, 0),
(14080, 14079, '14081', 14079, 2, 0),
(14081, 14080, NULL, 14079, 3, 0),
(14082, 0, '14083', 14082, 1, 0),
(14083, 14082, NULL, 14082, 2, 0),
(14113, 0, '14114', 14113, 1, 0),
(14114, 14113, '14115', 14113, 2, 0),
(14115, 14114, '14116', 14113, 3, 0),
(14116, 14115, '14117', 14113, 4, 0),
(14117, 14116, NULL, 14113, 5, 0),
(14128, 0, '14132', 14128, 1, 0),
(14132, 14128, '14135', 14128, 2, 0),
(14135, 14132, '14136', 14128, 3, 0),
(14136, 14135, '14137', 14128, 4, 0),
(14137, 14136, NULL, 14128, 5, 0),
(14138, 0, '14139', 14138, 1, 0),
(14139, 14138, '14140', 14138, 2, 0),
(14140, 14139, '14141', 14138, 3, 0),
(14141, 14140, '14142', 14138, 4, 0),
(14142, 14141, NULL, 14138, 5, 0),
(14144, 0, '14148', 14144, 1, 0),
(14148, 14144, NULL, 14144, 2, 0),
(14156, 0, '14160', 14156, 1, 0),
(14160, 14156, '14161', 14156, 2, 0),
(14161, 14160, NULL, 14156, 3, 0),
(14158, 0, '14159', 14158, 1, 0),
(14159, 14158, NULL, 14158, 2, 0),
(14162, 0, '14163', 14162, 1, 0),
(14163, 14162, '14164', 14162, 2, 0),
(14164, 14163, NULL, 14162, 3, 0),
(13733, 0, '13865', 13733, 1, 0),
(13865, 13733, '13866', 13733, 2, 0),
(13866, 13865, NULL, 13733, 3, 0),
(14168, 0, '14169', 14168, 1, 0),
(14169, 14168, NULL, 14168, 2, 0),
(14174, 0, '14175', 14174, 1, 0),
(14175, 14174, '14176', 14174, 2, 0),
(14176, 14175, NULL, 14174, 3, 0),
(14186, 0, '14190', 14186, 1, 0),
(14190, 14186, '14193', 14186, 2, 0),
(14193, 14190, '14194', 14186, 3, 0),
(14194, 14193, '14195', 14186, 4, 0),
(14195, 14194, NULL, 14186, 5, 0),
(14531, 0, '14774', 14531, 1, 0),
(14774, 14531, NULL, 14531, 2, 0),
(14520, 0, '14780', 14520, 1, 0),
(14780, 14520, '14781', 14520, 2, 0),
(14781, 14780, '14782', 14520, 3, 0),
(14782, 14781, '14783', 14520, 4, 0),
(14783, 14782, NULL, 14520, 5, 0),
(14522, 0, '14788', 14522, 1, 0),
(14788, 14522, '14789', 14522, 2, 0),
(14789, 14788, '14790', 14522, 3, 0),
(14790, 14789, '14791', 14522, 4, 0),
(14791, 14790, NULL, 14522, 5, 0),
(14748, 0, '14768', 14748, 1, 0),
(14768, 14748, '14769', 14748, 2, 0),
(14769, 14768, NULL, 14748, 3, 0),
(14749, 0, '14767', 14749, 1, 0),
(14767, 14749, NULL, 14749, 2, 0),
(14524, 0, '14525', 14524, 1, 0),
(14525, 14524, '14526', 14524, 2, 0),
(14526, 14525, '14527', 14524, 3, 0),
(14527, 14526, '14528', 14524, 4, 0),
(14528, 14527, NULL, 14524, 5, 0),
(14747, 0, '14770', 14747, 1, 0),
(14770, 14747, '14771', 14747, 2, 0),
(14771, 14770, NULL, 14747, 3, 0),
(14521, 0, '14776', 14521, 1, 0),
(14776, 14521, '14777', 14521, 2, 0),
(14777, 14776, NULL, 14521, 3, 0),
(14750, 0, '14772', 14750, 1, 0),
(14772, 14750, NULL, 14750, 2, 0),
(14523, 0, '14784', 14523, 1, 0),
(14784, 14523, '14785', 14523, 2, 0),
(14785, 14784, '14786', 14523, 3, 0),
(14786, 14785, '14787', 14523, 4, 0),
(14787, 14786, NULL, 14523, 5, 0),
(14892, 0, '15362', 14892, 1, 0),
(15362, 14892, '15363', 14892, 2, 0),
(15363, 15362, NULL, 14892, 3, 0),
(14889, 0, '15008', 14889, 1, 0),
(15008, 14889, '15009', 14889, 2, 0),
(15009, 15008, '15010', 14889, 3, 0),
(15010, 15009, '15011', 14889, 4, 0),
(15011, 15010, NULL, 14889, 5, 0),
(14901, 0, '15028', 14901, 1, 0),
(15028, 14901, '15029', 14901, 2, 0),
(15029, 15028, '15030', 14901, 3, 0),
(15030, 15029, '15031', 14901, 4, 0),
(15031, 15030, NULL, 14901, 5, 0),
(14909, 0, '15017', 14909, 1, 0),
(15017, 14909, NULL, 14909, 2, 0),
(14898, 0, '15349', 14898, 1, 0),
(15349, 14898, '15354', 14898, 2, 0),
(15354, 15349, '15355', 14898, 3, 0),
(15355, 15354, '15356', 14898, 4, 0),
(15356, 15355, NULL, 14898, 5, 0),
(14908, 0, '15020', 14908, 1, 0),
(15020, 14908, '17191', 14908, 2, 0),
(17191, 15020, NULL, 14908, 3, 0),
(14912, 0, '15013', 14912, 1, 0),
(15013, 14912, '15014', 14912, 2, 0),
(15014, 15013, NULL, 14912, 3, 0),
(14913, 0, '15012', 14913, 1, 0),
(15012, 14913, NULL, 14913, 2, 0),
(27900, 0, '27901', 27900, 1, 0),
(27901, 27900, '27902', 27900, 2, 0),
(27902, 27901, '27903', 27900, 3, 0),
(27903, 27902, '27904', 27900, 4, 0),
(27904, 27903, NULL, 27900, 5, 0),
(14911, 0, '15018', 14911, 1, 0),
(15018, 14911, NULL, 14911, 2, 0),
(15058, 0, '15059', 15058, 1, 0),
(15059, 15058, '15060', 15058, 2, 0),
(15060, 15059, NULL, 15058, 3, 0),
(15257, 0, '15331', 15257, 1, 0),
(15331, 15257, '15332', 15257, 2, 0),
(15332, 15331, '15333', 15257, 3, 0),
(15333, 15332, '15334', 15257, 4, 0),
(15334, 15333, NULL, 15257, 5, 0),
(15259, 0, '15307', 15259, 1, 0),
(15307, 15259, '15308', 15259, 2, 0),
(15308, 15307, '15309', 15259, 3, 0),
(15309, 15308, '15310', 15259, 4, 0),
(15310, 15309, NULL, 15259, 5, 0),
(15260, 0, '15327', 15260, 1, 0),
(15327, 15260, '15328', 15260, 2, 0),
(15328, 15327, '15329', 15260, 3, 0),
(15329, 15328, '15330', 15260, 4, 0),
(15330, 15329, NULL, 15260, 5, 0),
(15268, 0, '15323', 15268, 1, 0),
(15323, 15268, '15324', 15268, 2, 0),
(15324, 15323, '15325', 15268, 3, 0),
(15325, 15324, '15326', 15268, 4, 0),
(15326, 15325, NULL, 15268, 5, 0),
(15270, 0, '15335', 15270, 1, 0),
(15335, 15270, '15336', 15270, 2, 0),
(15336, 15335, '15337', 15270, 3, 0),
(15337, 15336, '15338', 15270, 4, 0),
(15338, 15337, NULL, 15270, 5, 0),
(15318, 0, '15272', 15318, 1, 0),
(15272, 15318, '15320', 15318, 2, 0),
(15320, 15272, NULL, 15318, 3, 0),
(15273, 0, '15312', 15273, 1, 0),
(15312, 15273, '15313', 15273, 2, 0),
(15313, 15312, '15314', 15273, 3, 0),
(15314, 15313, '15316', 15273, 4, 0),
(15316, 15314, NULL, 15273, 5, 0),
(15275, 0, '15317', 15275, 1, 0),
(15317, 15275, NULL, 15275, 2, 0),
(15274, 0, '15311', 15274, 1, 0),
(15311, 15274, NULL, 15274, 2, 0),
(15392, 0, '15448', 15392, 1, 0),
(15448, 15392, NULL, 15392, 2, 0),
(16038, 0, '16160', 16038, 1, 0),
(16160, 16038, '16161', 16038, 2, 0),
(16161, 16160, NULL, 16038, 3, 0),
(16041, 0, '16117', 16041, 1, 0),
(16117, 16041, '16118', 16041, 2, 0),
(16118, 16117, '16119', 16041, 3, 0),
(16119, 16118, '16120', 16041, 4, 0),
(16120, 16119, NULL, 16041, 5, 0),
(16035, 0, '16105', 16035, 1, 0),
(16105, 16035, '16106', 16035, 2, 0),
(16106, 16105, '16107', 16035, 3, 0),
(16107, 16106, '16108', 16035, 4, 0),
(16108, 16107, NULL, 16035, 5, 0),
(16039, 0, '16109', 16039, 1, 0),
(16109, 16039, '16110', 16039, 2, 0),
(16110, 16109, '16111', 16039, 3, 0),
(16111, 16110, '16112', 16039, 4, 0),
(16112, 16111, NULL, 16039, 5, 0),
(16086, 0, '16544', 16086, 1, 0),
(16544, 16086, NULL, 16086, 2, 0),
(16043, 0, '16130', 16043, 1, 0),
(16130, 16043, NULL, 16043, 2, 0),
(16040, 0, '16113', 16040, 1, 0),
(16113, 16040, '16114', 16040, 2, 0),
(16114, 16113, '16115', 16040, 3, 0),
(16115, 16114, '16116', 16040, 4, 0),
(16116, 16115, NULL, 16040, 5, 0),
(16176, 0, '16235', 16176, 1, 0),
(16235, 16176, '16240', 16176, 2, 0),
(16240, 16235, NULL, 16176, 3, 0),
(16180, 0, '16196', 16180, 1, 0),
(16196, 16180, '16198', 16180, 2, 0),
(16198, 16196, NULL, 16180, 3, 0),
(16182, 0, '16226', 16182, 1, 0),
(16226, 16182, '16227', 16182, 2, 0),
(16227, 16226, '16228', 16182, 3, 0),
(16228, 16227, '16229', 16182, 4, 0),
(16229, 16228, NULL, 16182, 5, 0),
(16181, 0, '16230', 16181, 1, 0),
(16230, 16181, '16232', 16181, 2, 0),
(16232, 16230, '16233', 16181, 3, 0),
(16233, 16232, '16234', 16181, 4, 0),
(16234, 16233, NULL, 16181, 5, 0),
(16187, 0, '16205', 16187, 1, 0),
(16205, 16187, '16206', 16187, 2, 0),
(16206, 16205, '16207', 16187, 3, 0),
(16207, 16206, '16208', 16187, 4, 0),
(16208, 16207, NULL, 16187, 5, 0),
(16184, 0, '16209', 16184, 1, 0),
(16209, 16184, NULL, 16184, 2, 0),
(16178, 0, '16210', 16178, 1, 0),
(16210, 16178, '16211', 16178, 2, 0),
(16211, 16210, '16212', 16178, 3, 0),
(16212, 16211, '16213', 16178, 4, 0),
(16213, 16212, NULL, 16178, 5, 0),
(16179, 0, '16214', 16179, 1, 0),
(16214, 16179, '16215', 16179, 2, 0),
(16215, 16214, '16216', 16179, 3, 0),
(16216, 16215, '16217', 16179, 4, 0),
(16217, 16216, NULL, 16179, 5, 0),
(16194, 0, '16218', 16194, 1, 0),
(16218, 16194, '16219', 16194, 2, 0),
(16219, 16218, '16220', 16194, 3, 0),
(16220, 16219, '16221', 16194, 4, 0),
(16221, 16220, NULL, 16194, 5, 0),
(16173, 0, '16222', 16173, 1, 0),
(16222, 16173, '16223', 16173, 2, 0),
(16223, 16222, '16224', 16173, 3, 0),
(16224, 16223, '16225', 16173, 4, 0),
(16225, 16224, NULL, 16173, 5, 0),
(16254, 0, '16271', 16254, 1, 0),
(16271, 16254, '16272', 16254, 2, 0),
(16272, 16271, '16273', 16254, 3, 0),
(16273, 16272, '16274', 16254, 4, 0),
(16274, 16273, NULL, 16254, 5, 0),
(16256, 0, '16281', 16256, 1, 0),
(16281, 16256, '16282', 16256, 2, 0),
(16282, 16281, '16283', 16256, 3, 0),
(16283, 16282, '16284', 16256, 4, 0),
(16284, 16283, NULL, 16256, 5, 0),
(16262, 0, '16287', 16262, 1, 0),
(16287, 16262, NULL, 16262, 2, 0),
(16261, 0, '16290', 16261, 1, 0),
(16290, 16261, '16291', 16261, 2, 0),
(16291, 16290, NULL, 16261, 3, 0),
(16258, 0, '16293', 16258, 1, 0),
(16293, 16258, NULL, 16258, 2, 0),
(16259, 0, '16295', 16259, 1, 0),
(16295, 16259, NULL, 16259, 2, 0),
(16266, 0, '29079', 16266, 1, 0),
(29079, 16266, '29080', 16266, 2, 0),
(29080, 29079, NULL, 16266, 3, 0),
(16253, 0, '16298', 16253, 1, 0),
(16298, 16253, '16299', 16253, 2, 0),
(16299, 16298, '16300', 16253, 3, 0),
(16300, 16299, '16301', 16253, 4, 0),
(16301, 16300, NULL, 16253, 5, 0),
(16255, 0, '16302', 16255, 1, 0),
(16302, 16255, '16303', 16255, 2, 0),
(16303, 16302, '16304', 16255, 3, 0),
(16304, 16303, '16305', 16255, 4, 0),
(16305, 16304, NULL, 16255, 5, 0),
(17485, 0, '17486', 17485, 1, 0),
(17486, 17485, '17487', 17485, 2, 0),
(17487, 17486, '17488', 17485, 3, 0),
(17488, 17487, '17489', 17485, 4, 0),
(17489, 17488, NULL, 17485, 5, 0),
(16252, 0, '16306', 16252, 1, 0),
(16306, 16252, '16307', 16252, 2, 0),
(16307, 16306, '16308', 16252, 3, 0),
(16308, 16307, '16309', 16252, 4, 0),
(16309, 16308, NULL, 16252, 5, 0),
(12300, 0, '12959', 12300, 1, 0),
(12959, 12300, '12960', 12300, 2, 0),
(12960, 12959, '12961', 12300, 3, 0),
(12961, 12960, '12962', 12300, 4, 0),
(12962, 12961, NULL, 12300, 5, 0),
(16487, 0, '16489', 16487, 1, 0),
(16489, 16487, '16492', 16487, 2, 0),
(16492, 16489, NULL, 16487, 3, 0),
(16493, 0, '16494', 16493, 1, 0),
(16494, 16493, NULL, 16493, 2, 0),
(16513, 0, '16514', 16513, 1, 0),
(16514, 16513, '16515', 16513, 2, 0),
(16515, 16514, '16719', 16513, 3, 0),
(16719, 16515, '16720', 16513, 4, 0),
(16720, 16719, NULL, 16513, 5, 0),
(16538, 0, '16539', 16538, 1, 0),
(16539, 16538, '16540', 16538, 2, 0),
(16540, 16539, '16541', 16538, 3, 0),
(16541, 16540, '16542', 16538, 4, 0),
(16542, 16541, NULL, 16538, 5, 0),
(16578, 0, '16579', 16578, 1, 0),
(16579, 16578, '16580', 16578, 2, 0),
(16580, 16579, '16581', 16578, 3, 0),
(16581, 16580, '16582', 16578, 4, 0),
(16582, 16581, NULL, 16578, 5, 0),
(16757, 0, '16758', 16757, 1, 0),
(16758, 16757, NULL, 16757, 2, 0),
(16814, 0, '16815', 16814, 1, 0),
(16815, 16814, '16816', 16814, 2, 0),
(16816, 16815, '16817', 16814, 3, 0),
(16817, 16816, '16818', 16814, 4, 0),
(16818, 16817, NULL, 16814, 5, 0),
(16821, 0, '16822', 16821, 1, 0),
(16822, 16821, NULL, 16821, 2, 0),
(16819, 0, '16820', 16819, 1, 0),
(16820, 16819, NULL, 16819, 2, 0),
(16836, 0, '16839', 16836, 1, 0),
(16839, 16836, '16840', 16836, 2, 0),
(16840, 16839, NULL, 16836, 3, 0),
(16845, 0, '16846', 16845, 1, 0),
(16846, 16845, '16847', 16845, 2, 0),
(16847, 16846, NULL, 16845, 3, 0),
(16850, 0, '16923', 16850, 1, 0),
(16923, 16850, '16924', 16850, 2, 0),
(16924, 16923, NULL, 16850, 3, 0),
(16918, 0, '16919', 16918, 1, 0),
(16919, 16918, '16920', 16918, 2, 0),
(16920, 16919, NULL, 16918, 3, 0),
(16896, 0, '16897', 16896, 1, 0),
(16897, 16896, '16899', 16896, 2, 0),
(16899, 16897, '16900', 16896, 3, 0),
(16900, 16899, '16901', 16896, 4, 0),
(16901, 16900, NULL, 16896, 5, 0),
(16909, 0, '16910', 16909, 1, 0),
(16910, 16909, '16911', 16909, 2, 0),
(16911, 16910, '16912', 16909, 3, 0),
(16912, 16911, '16913', 16909, 4, 0),
(16913, 16912, NULL, 16909, 5, 0),
(16929, 0, '16930', 16929, 1, 0),
(16930, 16929, '16931', 16929, 2, 0),
(16931, 16930, NULL, 16929, 3, 0),
(16858, 0, '16859', 16858, 1, 0),
(16859, 16858, '16860', 16858, 2, 0),
(16860, 16859, '16861', 16858, 3, 0),
(16861, 16860, '16862', 16858, 4, 0),
(16862, 16861, NULL, 16858, 5, 0),
(16934, 0, '16935', 16934, 1, 0),
(16935, 16934, '16936', 16934, 2, 0),
(16936, 16935, '16937', 16934, 3, 0),
(16937, 16936, '16938', 16934, 4, 0),
(16938, 16937, NULL, 16934, 5, 0),
(16940, 0, '16941', 16940, 1, 0),
(16941, 16940, NULL, 16940, 2, 0),
(16942, 0, '16943', 16942, 1, 0),
(16943, 16942, '16944', 16942, 2, 0),
(16944, 16943, NULL, 16942, 3, 0),
(16947, 0, '16948', 16947, 1, 0),
(16948, 16947, '16949', 16947, 2, 0),
(16949, 16948, NULL, 16947, 3, 0),
(37116, 0, '37117', 37116, 1, 0),
(37117, 37116, NULL, 37116, 2, 0),
(16966, 0, '16968', 16966, 1, 0),
(16968, 16966, NULL, 16966, 2, 0),
(16972, 0, '16974', 16972, 1, 0),
(16974, 16972, '16975', 16972, 2, 0),
(16975, 16974, NULL, 16972, 3, 0),
(16998, 0, '16999', 16998, 1, 0),
(16999, 16998, NULL, 16998, 2, 0),
(17002, 0, '24866', 17002, 1, 0),
(24866, 17002, NULL, 17002, 2, 0),
(17003, 0, '17004', 17003, 1, 0),
(17004, 17003, '17005', 17003, 2, 0),
(17005, 17004, '17006', 17003, 3, 0),
(17006, 17005, '24894', 17003, 4, 0),
(24894, 17006, NULL, 17003, 5, 0),
(17050, 0, '17051', 17050, 1, 0),
(17051, 17050, '17053', 17050, 2, 0),
(17053, 17051, '17054', 17050, 3, 0),
(17054, 17053, '17055', 17050, 4, 0),
(17055, 17054, NULL, 17050, 5, 0),
(17056, 0, '17058', 17056, 1, 0),
(17058, 17056, '17059', 17056, 2, 0),
(17059, 17058, '17060', 17056, 3, 0),
(17060, 17059, '17061', 17056, 4, 0),
(17061, 17060, NULL, 17056, 5, 0),
(17063, 0, '17065', 17063, 1, 0),
(17065, 17063, '17066', 17063, 2, 0),
(17066, 17065, '17067', 17063, 3, 0),
(17067, 17066, '17068', 17063, 4, 0),
(17068, 17067, NULL, 17063, 5, 0),
(17069, 0, '17070', 17069, 1, 0),
(17070, 17069, '17071', 17069, 2, 0),
(17071, 17070, '17072', 17069, 3, 0),
(17072, 17071, '17073', 17069, 4, 0),
(17073, 17072, NULL, 17069, 5, 0),
(17074, 0, '17075', 17074, 1, 0),
(17075, 17074, '17076', 17074, 2, 0),
(17076, 17075, '17077', 17074, 3, 0),
(17077, 17076, '17078', 17074, 4, 0),
(17078, 17077, NULL, 17074, 5, 0),
(16833, 0, '16834', 16833, 1, 0),
(16834, 16833, '16835', 16833, 2, 0),
(16835, 16834, NULL, 16833, 3, 0),
(17104, 0, '24943', 17104, 1, 0),
(24943, 17104, '24944', 17104, 2, 0),
(24944, 24943, '24945', 17104, 3, 0),
(24945, 24944, '24946', 17104, 4, 0),
(24946, 24945, NULL, 17104, 5, 0),
(17106, 0, '17107', 17106, 1, 0),
(17107, 17106, '17108', 17106, 2, 0),
(17108, 17107, NULL, 17106, 3, 0),
(17111, 0, '17112', 17111, 1, 0),
(17112, 17111, '17113', 17111, 2, 0),
(17113, 17112, NULL, 17111, 3, 0),
(17118, 0, '17119', 17118, 1, 0),
(17119, 17118, '17120', 17118, 2, 0),
(17120, 17119, '17121', 17118, 3, 0),
(17121, 17120, '17122', 17118, 4, 0),
(17122, 17121, NULL, 17118, 5, 0),
(17123, 0, '17124', 17123, 1, 0),
(17124, 17123, NULL, 17123, 2, 0),
(24968, 0, '24969', 24968, 1, 0),
(24969, 24968, '24970', 24968, 2, 0),
(24970, 24969, '24971', 24968, 3, 0),
(24971, 24970, '24972', 24968, 4, 0),
(24972, 24971, NULL, 24968, 5, 0),
(17322, 0, '17323', 17322, 1, 0),
(17323, 17322, NULL, 17322, 2, 0),
(17245, 0, '17247', 17245, 1, 0),
(17247, 17245, '17248', 17245, 2, 0),
(17248, 17247, '17249', 17245, 3, 0),
(17249, 17248, NULL, 17245, 4, 0),
(17778, 0, '17779', 17778, 1, 0),
(17779, 17778, '17780', 17778, 2, 0),
(17780, 17779, '17781', 17778, 3, 0),
(17781, 17780, '17782', 17778, 4, 0),
(17782, 17781, NULL, 17778, 5, 0),
(17788, 0, '17789', 17788, 1, 0),
(17789, 17788, '17790', 17788, 2, 0),
(17790, 17789, '17791', 17788, 3, 0),
(17791, 17790, '17792', 17788, 4, 0),
(17792, 17791, NULL, 17788, 5, 0),
(17793, 0, '17796', 17793, 1, 0),
(17796, 17793, '17801', 17793, 2, 0),
(17801, 17796, '17802', 17793, 3, 0),
(17802, 17801, '17803', 17793, 4, 0),
(17803, 17802, NULL, 17793, 5, 0),
(17815, 0, '17833', 17815, 1, 0),
(17833, 17815, '17834', 17815, 2, 0),
(17834, 17833, '17835', 17815, 3, 0),
(17835, 17834, '17836', 17815, 4, 0),
(17836, 17835, NULL, 17815, 5, 0),
(17917, 0, '17918', 17917, 1, 0),
(17918, 17917, NULL, 17917, 2, 0),
(17927, 0, '17929', 17927, 1, 0),
(17929, 17927, '17930', 17927, 2, 0),
(17930, 17929, NULL, 17927, 3, 0),
(17954, 0, '17955', 17954, 1, 0),
(17955, 17954, '17956', 17954, 2, 0),
(17956, 17955, '17957', 17954, 3, 0),
(17957, 17956, '17958', 17954, 4, 0),
(17958, 17957, NULL, 17954, 5, 0),
(18130, 0, '18131', 18130, 1, 0),
(18131, 18130, '18132', 18130, 2, 0),
(18132, 18131, '18133', 18130, 3, 0),
(18133, 18132, '18134', 18130, 4, 0),
(18134, 18133, NULL, 18130, 5, 0),
(18119, 0, '18120', 18119, 1, 0),
(18120, 18119, '18121', 18119, 2, 0),
(18121, 18120, '18122', 18119, 3, 0),
(18122, 18121, '18123', 18119, 4, 0),
(18123, 18122, NULL, 18119, 5, 0),
(18126, 0, '18127', 18126, 1, 0),
(18127, 18126, NULL, 18126, 2, 0),
(18128, 0, '18129', 18128, 1, 0),
(18129, 18128, NULL, 18128, 2, 0),
(18135, 0, '18136', 18135, 1, 0),
(18136, 18135, NULL, 18135, 2, 0),
(18096, 0, '18073', 18096, 1, 0),
(18073, 18096, NULL, 18096, 2, 0),
(17783, 0, '17784', 17783, 1, 0),
(17784, 17783, '17785', 17783, 2, 0),
(17785, 17784, '17786', 17783, 3, 0),
(17786, 17785, '17787', 17783, 4, 0),
(17787, 17786, NULL, 17783, 5, 0),
(18094, 0, '18095', 18094, 1, 0),
(18095, 18094, NULL, 18094, 2, 0),
(17810, 0, '17811', 17810, 1, 0),
(17811, 17810, '17812', 17810, 2, 0),
(17812, 17811, '17813', 17810, 3, 0),
(17813, 17812, '17814', 17810, 4, 0),
(17814, 17813, NULL, 17810, 5, 0),
(17804, 0, '17805', 17804, 1, 0),
(17805, 17804, NULL, 17804, 2, 0),
(18174, 0, '18175', 18174, 1, 0),
(18175, 18174, '18176', 18174, 2, 0),
(18176, 18175, '18177', 18174, 3, 0),
(18177, 18176, '18178', 18174, 4, 0),
(18178, 18177, NULL, 18174, 5, 0),
(18179, 0, '18180', 18179, 1, 0),
(18180, 18179, NULL, 18179, 2, 0),
(18182, 0, '18183', 18182, 1, 0),
(18183, 18182, NULL, 18182, 2, 0),
(18218, 0, '18219', 18218, 1, 0),
(18219, 18218, NULL, 18218, 2, 0),
(18271, 0, '18272', 18271, 1, 0),
(18272, 18271, '18273', 18271, 2, 0),
(18273, 18272, '18274', 18271, 3, 0),
(18274, 18273, '18275', 18271, 4, 0),
(18275, 18274, NULL, 18271, 5, 0),
(18213, 0, '18372', 18213, 1, 0),
(18372, 18213, NULL, 18213, 2, 0),
(18427, 0, '18428', 18427, 1, 0),
(18428, 18427, '18429', 18427, 2, 0),
(18429, 18428, NULL, 18427, 3, 0),
(14171, 0, '14172', 14171, 1, 0),
(14172, 14171, '14173', 14171, 2, 0),
(14173, 14172, NULL, 14171, 3, 0),
(18459, 0, '18460', 18459, 1, 0),
(18460, 18459, NULL, 18459, 2, 0),
(18462, 0, '18463', 18462, 1, 0),
(18463, 18462, '18464', 18462, 2, 0),
(18464, 18463, NULL, 18462, 3, 0),
(18530, 0, '18531', 18530, 1, 0),
(18531, 18530, '18533', 18530, 2, 0),
(18533, 18531, '18534', 18530, 3, 0),
(18534, 18533, '18535', 18530, 4, 0),
(18535, 18534, NULL, 18530, 5, 0),
(18551, 0, '18552', 18551, 1, 0),
(18552, 18551, '18553', 18551, 2, 0),
(18553, 18552, '18554', 18551, 3, 0),
(18554, 18553, '18555', 18551, 4, 0),
(18555, 18554, NULL, 18551, 5, 0),
(18544, 0, '18547', 18544, 1, 0),
(18547, 18544, '18548', 18544, 2, 0),
(18548, 18547, '18549', 18544, 3, 0),
(18549, 18548, '18550', 18544, 4, 0),
(18550, 18549, NULL, 18544, 5, 0),
(18692, 0, '18693', 18692, 1, 0),
(18693, 18692, NULL, 18692, 2, 0),
(18694, 0, '18695', 18694, 1, 0),
(18695, 18694, '18696', 18694, 2, 0),
(18696, 18695, NULL, 18694, 3, 0),
(18697, 0, '18698', 18697, 1, 0),
(18698, 18697, '18699', 18697, 2, 0),
(18699, 18698, '18700', 18697, 3, 0),
(18700, 18699, '18701', 18697, 4, 0),
(18701, 18700, NULL, 18697, 5, 0),
(18703, 0, '18704', 18703, 1, 0),
(18704, 18703, NULL, 18703, 2, 0),
(18705, 0, '18706', 18705, 1, 0),
(18706, 18705, '18707', 18705, 2, 0),
(18707, 18706, NULL, 18705, 3, 0),
(18709, 0, '18710', 18709, 1, 0),
(18710, 18709, NULL, 18709, 2, 0),
(18748, 0, '18749', 18748, 1, 0),
(18749, 18748, '18750', 18748, 2, 0),
(18750, 18749, NULL, 18748, 3, 0),
(18731, 0, '18743', 18731, 1, 0),
(18743, 18731, '18744', 18731, 2, 0),
(18744, 18743, NULL, 18731, 3, 0),
(18754, 0, '18755', 18754, 1, 0),
(18755, 18754, '18756', 18754, 2, 0),
(18756, 18755, NULL, 18754, 3, 0),
(23785, 0, '23822', 23785, 1, 0),
(23822, 23785, '23823', 23785, 2, 0),
(23823, 23822, '23824', 23785, 3, 0),
(23824, 23823, '23825', 23785, 4, 0),
(23825, 23824, NULL, 23785, 5, 0),
(18767, 0, '18768', 18767, 1, 0),
(18768, 18767, NULL, 18767, 2, 0),
(18769, 0, '18770', 18769, 1, 0),
(18770, 18769, '18771', 18769, 2, 0),
(18771, 18770, '18772', 18769, 3, 0),
(18772, 18771, '18773', 18769, 4, 0),
(18773, 18772, NULL, 18769, 5, 0),
(35691, 0, '35692', 35691, 1, 0),
(35692, 35691, '35693', 35691, 2, 0),
(35693, 35692, NULL, 35691, 3, 0),
(18821, 0, '18822', 18821, 1, 0),
(18822, 18821, NULL, 18821, 2, 0),
(18827, 0, '18829', 18827, 1, 0),
(18829, 18827, NULL, 18827, 2, 0),
(19151, 0, '19152', 19151, 1, 0),
(19152, 19151, '19153', 19151, 2, 0),
(19153, 19152, NULL, 19151, 3, 0),
(19168, 0, '19180', 19168, 1, 0),
(19180, 19168, '19181', 19168, 2, 0),
(19181, 19180, '24296', 19168, 3, 0),
(24296, 19181, '24297', 19168, 4, 0),
(24297, 24296, NULL, 19168, 5, 0),
(19184, 0, '19387', 19184, 1, 0),
(19387, 19184, '19388', 19184, 2, 0),
(19388, 19387, NULL, 19184, 3, 0),
(19228, 0, '19232', 19228, 1, 0),
(19232, 19228, '19233', 19228, 2, 0),
(19233, 19232, NULL, 19228, 3, 0),
(19239, 0, '19245', 19239, 1, 0),
(19245, 19239, NULL, 19239, 2, 0),
(19286, 0, '19287', 19286, 1, 0),
(19287, 19286, NULL, 19286, 2, 0),
(19290, 0, '19294', 19290, 1, 0),
(19294, 19290, '24283', 19290, 2, 0),
(24283, 19294, NULL, 19290, 3, 0),
(19295, 0, '19297', 19295, 1, 0),
(19297, 19295, '19298', 19295, 2, 0),
(19298, 19297, '19301', 19295, 3, 0),
(19301, 19298, '19300', 19295, 4, 0),
(19300, 19301, NULL, 19295, 5, 0),
(19370, 0, '19371', 19370, 1, 0),
(19371, 19370, '19373', 19370, 2, 0),
(19373, 19371, NULL, 19370, 3, 0),
(19376, 0, '19377', 19376, 1, 0),
(19377, 19376, NULL, 19376, 2, 0),
(19407, 0, '19412', 19407, 1, 0),
(19412, 19407, '19413', 19407, 2, 0),
(19413, 19412, '19414', 19407, 3, 0),
(19414, 19413, '19415', 19407, 4, 0),
(19415, 19414, NULL, 19407, 5, 0),
(19416, 0, '19417', 19416, 1, 0),
(19417, 19416, '19418', 19416, 2, 0),
(19418, 19417, '19419', 19416, 3, 0),
(19419, 19418, '19420', 19416, 4, 0),
(19420, 19419, NULL, 19416, 5, 0),
(19421, 0, '19422', 19421, 1, 0),
(19422, 19421, '19423', 19421, 2, 0),
(19423, 19422, '19424', 19421, 3, 0),
(19424, 19423, '19425', 19421, 4, 0),
(19425, 19424, NULL, 19421, 5, 0),
(19426, 0, '19427', 19426, 1, 0),
(19427, 19426, '19429', 19426, 2, 0),
(19429, 19427, '19430', 19426, 3, 0),
(19430, 19429, '19431', 19426, 4, 0),
(19431, 19430, NULL, 19426, 5, 0),
(19454, 0, '19455', 19454, 1, 0),
(19455, 19454, '19456', 19454, 2, 0),
(19456, 19455, '19457', 19454, 3, 0),
(19457, 19456, '19458', 19454, 4, 0),
(19458, 19457, NULL, 19454, 5, 0),
(19461, 0, '19462', 19461, 1, 0),
(19462, 19461, '24691', 19461, 2, 0),
(24691, 19462, NULL, 19461, 3, 0),
(19464, 0, '19465', 19464, 1, 0),
(19465, 19464, '19466', 19464, 2, 0),
(19466, 19465, '19467', 19464, 3, 0),
(19467, 19466, '19468', 19464, 4, 0),
(19468, 19467, NULL, 19464, 5, 0),
(19485, 0, '19487', 19485, 1, 0),
(19487, 19485, '19488', 19485, 2, 0),
(19488, 19487, '19489', 19485, 3, 0),
(19489, 19488, '19490', 19485, 4, 0),
(19490, 19489, NULL, 19485, 5, 0),
(35100, 0, '35102', 35100, 1, 0),
(35102, 35100, '35103', 35100, 2, 0),
(35103, 35102, NULL, 35100, 3, 0),
(19507, 0, '19508', 19507, 1, 0),
(19508, 19507, '19509', 19507, 2, 0),
(19509, 19508, '19510', 19507, 3, 0),
(19510, 19509, '19511', 19507, 4, 0),
(19511, 19510, NULL, 19507, 5, 0),
(19549, 0, '19550', 19549, 1, 0),
(19550, 19549, '19551', 19549, 2, 0),
(19551, 19550, NULL, 19549, 3, 0),
(19552, 0, '19553', 19552, 1, 0),
(19553, 19552, '19554', 19552, 2, 0),
(19554, 19553, '19555', 19552, 3, 0),
(19555, 19554, '19556', 19552, 4, 0),
(19556, 19555, NULL, 19552, 5, 0),
(19559, 0, '19560', 19559, 1, 0),
(19560, 19559, NULL, 19559, 2, 0),
(19572, 0, '19573', 19572, 1, 0),
(19573, 19572, NULL, 19572, 2, 0),
(19578, 0, '20895', 19578, 1, 0),
(20895, 19578, NULL, 19578, 2, 0),
(19583, 0, '19584', 19583, 1, 0),
(19584, 19583, '19585', 19583, 2, 0),
(19585, 19584, '19586', 19583, 3, 0),
(19586, 19585, '19587', 19583, 4, 0),
(19587, 19586, NULL, 19583, 5, 0),
(19590, 0, '19592', 19590, 1, 0),
(19592, 19590, NULL, 19590, 2, 0),
(19598, 0, '19599', 19598, 1, 0),
(19599, 19598, '19600', 19598, 2, 0),
(19600, 19599, '19601', 19598, 3, 0),
(19601, 19600, '19602', 19598, 4, 0),
(19602, 19601, NULL, 19598, 5, 0),
(19609, 0, '19610', 19609, 1, 0),
(19610, 19609, '19612', 19609, 2, 0),
(19612, 19610, NULL, 19609, 3, 0),
(19616, 0, '19617', 19616, 1, 0),
(19617, 19616, '19618', 19616, 2, 0),
(19618, 19617, '19619', 19616, 3, 0),
(19619, 19618, '19620', 19616, 4, 0),
(19620, 19619, NULL, 19616, 5, 0),
(19621, 0, '19622', 19621, 1, 0),
(19622, 19621, '19623', 19621, 2, 0),
(19623, 19622, '19624', 19621, 3, 0),
(19624, 19623, '19625', 19621, 4, 0),
(19625, 19624, NULL, 19621, 5, 0),
(20042, 0, '20045', 20042, 1, 0),
(20045, 20042, '20046', 20042, 2, 0),
(20046, 20045, '20047', 20042, 3, 0),
(20047, 20046, '20048', 20042, 4, 0),
(20048, 20047, NULL, 20042, 5, 0),
(20049, 0, '20056', 20049, 1, 0),
(20056, 20049, '20057', 20049, 2, 0),
(20057, 20056, '20058', 20049, 3, 0),
(20058, 20057, '20059', 20049, 4, 0),
(20059, 20058, NULL, 20049, 5, 0),
(20060, 0, '20061', 20060, 1, 0),
(20061, 20060, '20062', 20060, 2, 0),
(20062, 20061, '20063', 20060, 3, 0),
(20063, 20062, '20064', 20060, 4, 0),
(20064, 20063, NULL, 20060, 5, 0),
(20091, 0, '20092', 20091, 1, 0),
(20092, 20091, NULL, 20091, 2, 0),
(20101, 0, '20102', 20101, 1, 0),
(20102, 20101, '20103', 20101, 2, 0),
(20103, 20102, '20104', 20101, 3, 0),
(20104, 20103, '20105', 20101, 4, 0),
(20105, 20104, NULL, 20101, 5, 0),
(20111, 0, '20112', 20111, 1, 0),
(20112, 20111, '20113', 20111, 2, 0),
(20113, 20112, NULL, 20111, 3, 0),
(20117, 0, '20118', 20117, 1, 0),
(20118, 20117, '20119', 20117, 2, 0),
(20119, 20118, '20120', 20117, 3, 0),
(20120, 20119, '20121', 20117, 4, 0),
(20121, 20120, NULL, 20117, 5, 0),
(20127, 0, '20130', 20127, 1, 0),
(20130, 20127, '20135', 20127, 2, 0),
(20135, 20130, '20136', 20127, 3, 0),
(20136, 20135, '20137', 20127, 4, 0),
(20137, 20136, NULL, 20127, 5, 0),
(20138, 0, '20139', 20138, 1, 0),
(20139, 20138, '20140', 20138, 2, 0),
(20140, 20139, '20141', 20138, 3, 0),
(20141, 20140, '20142', 20138, 4, 0),
(20142, 20141, NULL, 20138, 5, 0),
(20143, 0, '20144', 20143, 1, 0),
(20144, 20143, '20145', 20143, 2, 0),
(20145, 20144, '20146', 20143, 3, 0),
(20146, 20145, '20147', 20143, 4, 0),
(20147, 20146, NULL, 20143, 5, 0),
(20148, 0, '20149', 20148, 1, 0),
(20149, 20148, '20150', 20148, 2, 0),
(20150, 20149, NULL, 20148, 3, 0),
(20174, 0, '20175', 20174, 1, 0),
(20175, 20174, NULL, 20174, 2, 0),
(20177, 0, '20179', 20177, 1, 0),
(20179, 20177, '20181', 20177, 2, 0),
(20181, 20179, '20180', 20177, 3, 0),
(20180, 20181, '20182', 20177, 4, 0),
(20182, 20180, NULL, 20177, 5, 0),
(20196, 0, '20197', 20196, 1, 0),
(20197, 20196, '20198', 20196, 2, 0),
(20198, 20197, '20199', 20196, 3, 0),
(20199, 20198, '20200', 20196, 4, 0),
(20200, 20199, NULL, 20196, 5, 0),
(20205, 0, '20206', 20205, 1, 0),
(20206, 20205, '20207', 20205, 2, 0),
(20207, 20206, '20209', 20205, 3, 0),
(20209, 20207, '20208', 20205, 4, 0),
(20208, 20209, NULL, 20205, 5, 0),
(20234, 0, '20235', 20234, 1, 0),
(20235, 20234, NULL, 20234, 2, 0),
(20237, 0, '20238', 20237, 1, 0),
(20238, 20237, '20239', 20237, 2, 0),
(20239, 20238, NULL, 20237, 3, 0),
(20244, 0, '20245', 20244, 1, 0),
(20245, 20244, NULL, 20244, 2, 0),
(20257, 0, '20258', 20257, 1, 0),
(20258, 20257, '20259', 20257, 2, 0),
(20259, 20258, '20260', 20257, 3, 0),
(20260, 20259, '20261', 20257, 4, 0),
(20261, 20260, NULL, 20257, 5, 0),
(20262, 0, '20263', 20262, 1, 0),
(20263, 20262, '20264', 20262, 2, 0),
(20264, 20263, '20265', 20262, 3, 0),
(20265, 20264, '20266', 20262, 4, 0),
(20266, 20265, NULL, 20262, 5, 0),
(20210, 0, '20212', 20210, 1, 0),
(20212, 20210, '20213', 20210, 2, 0),
(20213, 20212, '20214', 20210, 3, 0),
(20214, 20213, '20215', 20210, 4, 0),
(20215, 20214, NULL, 20210, 5, 0),
(20224, 0, '20225', 20224, 1, 0),
(20225, 20224, '20330', 20224, 2, 0),
(20330, 20225, '20331', 20224, 3, 0),
(20331, 20330, '20332', 20224, 4, 0),
(20332, 20331, NULL, 20224, 5, 0),
(20335, 0, '20336', 20335, 1, 0),
(20336, 20335, '20337', 20335, 2, 0),
(20337, 20336, NULL, 20335, 3, 0),
(20359, 0, '20360', 20359, 1, 0),
(20360, 20359, '20361', 20359, 2, 0),
(20361, 20360, NULL, 20359, 3, 0),
(20468, 0, '20469', 20468, 1, 0),
(20469, 20468, '20470', 20468, 2, 0),
(20470, 20469, NULL, 20468, 3, 0),
(20487, 0, '20488', 20487, 1, 0),
(20488, 20487, '20489', 20487, 2, 0),
(20489, 20488, NULL, 20487, 3, 0),
(20500, 0, '20501', 20500, 1, 0),
(20501, 20500, NULL, 20500, 2, 0),
(20502, 0, '20503', 20502, 1, 0),
(20503, 20502, NULL, 20502, 2, 0),
(20504, 0, '20505', 20504, 1, 0),
(20505, 20504, NULL, 20504, 2, 0),
(23584, 0, '23585', 23584, 1, 0),
(23585, 23584, '23586', 23584, 2, 0),
(23586, 23585, '23587', 23584, 3, 0),
(23587, 23586, '23588', 23584, 4, 0),
(23588, 23587, NULL, 23584, 5, 0),
(12298, 0, '12724', 12298, 1, 0),
(12724, 12298, '12725', 12298, 2, 0),
(12725, 12724, '12726', 12298, 3, 0),
(12726, 12725, '12727', 12298, 4, 0),
(12727, 12726, NULL, 12298, 5, 0),
(19159, 0, '19160', 19159, 1, 0),
(19160, 19159, NULL, 19159, 2, 0),
(19255, 0, '19256', 19255, 1, 0),
(19256, 19255, '19257', 19255, 2, 0),
(19257, 19256, '19258', 19255, 3, 0),
(19258, 19257, '19259', 19255, 4, 0),
(19259, 19258, NULL, 19255, 5, 0),
(24293, 0, '24294', 24293, 1, 0),
(24294, 24293, '24295', 24293, 2, 0),
(24295, 24294, NULL, 24293, 3, 0),
(35029, 0, '35030', 35029, 1, 0),
(35030, 35029, NULL, 35029, 2, 0),
(24443, 0, '19575', 24443, 1, 0),
(19575, 24443, NULL, 24443, 2, 0),
(20254, 0, '20255', 20254, 1, 0),
(20255, 20254, '20256', 20254, 2, 0),
(20256, 20255, NULL, 20254, 3, 0),
(5923, 0, '5924', 5923, 1, 0),
(5924, 5923, '5925', 5923, 2, 0),
(5925, 5924, '5926', 5923, 3, 0),
(5926, 5925, '25829', 5923, 4, 0),
(25829, 5926, NULL, 5923, 5, 0),
(9453, 0, '25836', 9453, 1, 0),
(25836, 9453, NULL, 9453, 2, 0),
(20096, 0, '20097', 20096, 1, 0),
(20097, 20096, '20098', 20096, 2, 0),
(20098, 20097, '20099', 20096, 3, 0),
(20099, 20098, '20100', 20096, 4, 0),
(20100, 20099, NULL, 20096, 5, 0),
(20189, 0, '20192', 20189, 1, 0),
(20192, 20189, '20193', 20189, 2, 0),
(20193, 20192, NULL, 20189, 3, 0),
(25956, 0, '25957', 25956, 1, 0),
(25957, 25956, NULL, 25956, 2, 0),
(9799, 0, '25988', 9799, 1, 0),
(25988, 9799, NULL, 9799, 2, 0),
(9452, 0, '26016', 9452, 1, 0),
(26016, 9452, '26021', 9452, 2, 0),
(26021, 26016, NULL, 9452, 3, 0),
(26022, 0, '26023', 26022, 1, 0),
(26023, 26022, '44414', 26022, 2, 0),
(44414, 26023, NULL, 26022, 3, 0),
(27789, 0, '27790', 27789, 1, 0),
(27790, 27789, NULL, 27789, 2, 0),
(27811, 0, '27815', 27811, 1, 0),
(27815, 27811, '27816', 27811, 2, 0),
(27816, 27815, NULL, 27811, 3, 0),
(27839, 0, '27840', 27839, 1, 0),
(27840, 27839, NULL, 27839, 2, 0),
(29074, 0, '29075', 29074, 1, 0),
(29075, 29074, '29076', 29074, 2, 0),
(29076, 29075, NULL, 29074, 3, 0),
(28996, 0, '28997', 28996, 1, 0),
(28997, 28996, '28998', 28996, 2, 0),
(28998, 28997, NULL, 28996, 3, 0),
(28999, 0, '29000', 28999, 1, 0),
(29000, 28999, NULL, 28999, 2, 0),
(29062, 0, '29064', 29062, 1, 0),
(29064, 29062, '29065', 29062, 2, 0),
(29065, 29064, NULL, 29062, 3, 0),
(29082, 0, '29084', 29082, 1, 0),
(29084, 29082, '29086', 29082, 2, 0),
(29086, 29084, '29087', 29082, 3, 0),
(29087, 29086, '29088', 29082, 4, 0),
(29088, 29087, NULL, 29082, 5, 0),
(30160, 0, '29179', 30160, 1, 0),
(29179, 30160, '29180', 30160, 2, 0),
(29180, 29179, NULL, 30160, 3, 0),
(29187, 0, '29189', 29187, 1, 0),
(29189, 29187, '29191', 29187, 2, 0),
(29191, 29189, NULL, 29187, 3, 0),
(29192, 0, '29193', 29192, 1, 0),
(29193, 29192, NULL, 29192, 2, 0),
(29206, 0, '29205', 29206, 1, 0),
(29205, 29206, '29202', 29206, 2, 0),
(29202, 29205, NULL, 29206, 3, 0),
(29438, 0, '29439', 29438, 1, 0),
(29439, 29438, '29440', 29438, 2, 0),
(29440, 29439, NULL, 29438, 3, 0),
(29441, 0, '29444', 29441, 1, 0),
(29444, 29441, '29445', 29441, 2, 0),
(29445, 29444, '29446', 29441, 3, 0),
(29446, 29445, '29447', 29441, 4, 0),
(29447, 29446, NULL, 29441, 5, 0),
(29593, 0, '29594', 29593, 1, 0),
(29594, 29593, '29595', 29593, 2, 0),
(29595, 29594, NULL, 29593, 3, 0),
(29140, 0, '29143', 29140, 1, 0),
(29143, 29140, '29144', 29140, 2, 0),
(29144, 29143, '29145', 29140, 3, 0),
(29145, 29144, '29146', 29140, 4, 0),
(29146, 29145, NULL, 29140, 5, 0),
(29598, 0, '29599', 29598, 1, 0),
(29599, 29598, '29600', 29598, 2, 0),
(29600, 29599, NULL, 29598, 3, 0),
(29721, 0, '29776', 29721, 1, 0),
(29776, 29721, NULL, 29721, 2, 0),
(29590, 0, '29591', 29590, 1, 0),
(29591, 29590, '29592', 29590, 2, 0),
(29592, 29591, NULL, 29590, 3, 0),
(29759, 0, '29760', 29759, 1, 0),
(29760, 29759, '29761', 29759, 2, 0),
(29761, 29760, '29762', 29759, 3, 0),
(29762, 29761, '29763', 29759, 4, 0),
(29763, 29762, NULL, 29759, 5, 0),
(29787, 0, '29790', 29787, 1, 0),
(29790, 29787, '29792', 29787, 2, 0),
(29792, 29790, NULL, 29787, 3, 0),
(29723, 0, '29724', 29723, 1, 0),
(29724, 29723, '29725', 29723, 2, 0),
(29725, 29724, NULL, 29723, 3, 0),
(29834, 0, '29838', 29834, 1, 0),
(29838, 29834, NULL, 29834, 2, 0),
(29836, 0, '29859', 29836, 1, 0),
(29859, 29836, NULL, 29836, 2, 0),
(32477, 0, '32483', 32477, 1, 0),
(32483, 32477, '32484', 32477, 2, 0),
(32484, 32483, NULL, 32477, 3, 0),
(30054, 0, '30057', 30054, 1, 0),
(30057, 30054, NULL, 30054, 2, 0),
(30060, 0, '30061', 30060, 1, 0),
(30061, 30060, '30062', 30060, 2, 0),
(30062, 30061, '30063', 30060, 3, 0),
(30063, 30062, '30064', 30060, 4, 0),
(30064, 30063, NULL, 30060, 5, 0),
(30143, 0, '30144', 30143, 1, 0),
(30144, 30143, '30145', 30143, 2, 0),
(30145, 30144, NULL, 30143, 3, 0),
(30242, 0, '30245', 30242, 1, 0),
(30245, 30242, '30246', 30242, 2, 0),
(30246, 30245, '30247', 30242, 3, 0),
(30247, 30246, '30248', 30242, 4, 0),
(30248, 30247, NULL, 30242, 5, 0),
(30288, 0, '30289', 30288, 1, 0),
(30289, 30288, '30290', 30288, 2, 0),
(30290, 30289, '30291', 30288, 3, 0),
(30291, 30290, '30292', 30288, 4, 0),
(30292, 30291, NULL, 30288, 5, 0),
(30293, 0, '30295', 30293, 1, 0),
(30295, 30293, '30296', 30293, 2, 0),
(30296, 30295, NULL, 30293, 3, 0),
(30299, 0, '30301', 30299, 1, 0),
(30301, 30299, '30302', 30299, 2, 0),
(30302, 30301, NULL, 30299, 3, 0),
(30319, 0, '30320', 30319, 1, 0),
(30320, 30319, '30321', 30319, 2, 0),
(30321, 30320, NULL, 30319, 3, 0),
(30326, 0, '30327', 30326, 1, 0),
(30327, 30326, '30328', 30326, 2, 0),
(30328, 30327, NULL, 30326, 3, 0),
(30664, 0, '30665', 30664, 1, 0),
(30665, 30664, '30666', 30664, 2, 0),
(30666, 30665, '30667', 30664, 3, 0),
(30667, 30666, '30668', 30664, 4, 0),
(30668, 30667, NULL, 30664, 5, 0),
(30669, 0, '30670', 30669, 1, 0),
(30670, 30669, '30671', 30669, 2, 0),
(30671, 30670, NULL, 30669, 3, 0),
(30672, 0, '30673', 30672, 1, 0),
(30673, 30672, '30674', 30672, 2, 0),
(30674, 30673, NULL, 30672, 3, 0),
(30675, 0, '30678', 30675, 1, 0),
(30678, 30675, '30679', 30675, 2, 0),
(30679, 30678, '30680', 30675, 3, 0),
(30680, 30679, '30681', 30675, 4, 0),
(30681, 30680, NULL, 30675, 5, 0),
(30802, 0, '30808', 30802, 1, 0),
(30808, 30802, '30809', 30802, 2, 0),
(30809, 30808, '30810', 30802, 3, 0),
(30810, 30809, '30811', 30802, 4, 0),
(30811, 30810, NULL, 30802, 5, 0),
(30812, 0, '30813', 30812, 1, 0),
(30813, 30812, '30814', 30812, 2, 0),
(30814, 30813, NULL, 30812, 3, 0),
(30816, 0, '30818', 30816, 1, 0),
(30818, 30816, '30819', 30816, 2, 0),
(30819, 30818, NULL, 30816, 3, 0),
(30864, 0, '30865', 30864, 1, 0),
(30865, 30864, '30866', 30864, 2, 0),
(30866, 30865, NULL, 30864, 3, 0),
(30867, 0, '30868', 30867, 1, 0),
(30868, 30867, '30869', 30867, 2, 0),
(30869, 30868, NULL, 30867, 3, 0),
(30872, 0, '30873', 30872, 1, 0),
(30873, 30872, NULL, 30872, 2, 0),
(30881, 0, '30883', 30881, 1, 0),
(30883, 30881, '30884', 30881, 2, 0),
(30884, 30883, '30885', 30881, 3, 0),
(30885, 30884, '30886', 30881, 4, 0),
(30886, 30885, NULL, 30881, 5, 0),
(30892, 0, '30893', 30892, 1, 0),
(30893, 30892, NULL, 30892, 2, 0),
(30894, 0, '30895', 30894, 1, 0),
(30895, 30894, NULL, 30894, 2, 0),
(30902, 0, '30903', 30902, 1, 0),
(30903, 30902, '30904', 30902, 2, 0),
(30904, 30903, '30905', 30902, 3, 0),
(30905, 30904, '30906', 30902, 4, 0),
(30906, 30905, NULL, 30902, 5, 0),
(30919, 0, '30920', 30919, 1, 0),
(30920, 30919, NULL, 30919, 2, 0),
(31122, 0, '31123', 31122, 1, 0),
(31123, 31122, NULL, 31122, 2, 0),
(31124, 0, '31126', 31124, 1, 0),
(31126, 31124, NULL, 31124, 2, 0),
(31130, 0, '31131', 31130, 1, 0),
(31131, 31130, NULL, 31130, 2, 0),
(31211, 0, '31212', 31211, 1, 0),
(31212, 31211, '31213', 31211, 2, 0),
(31213, 31212, NULL, 31211, 3, 0),
(31216, 0, '31217', 31216, 1, 0),
(31217, 31216, '31218', 31216, 2, 0),
(31218, 31217, '31219', 31216, 3, 0),
(31219, 31218, '31220', 31216, 4, 0),
(31220, 31219, NULL, 31216, 5, 0),
(31221, 0, '31222', 31221, 1, 0),
(31222, 31221, '31223', 31221, 2, 0),
(31223, 31222, NULL, 31221, 3, 0),
(31226, 0, '31227', 31226, 1, 0),
(31227, 31226, NULL, 31226, 2, 0),
(31233, 0, '31239', 31233, 1, 0),
(31239, 31233, '31240', 31233, 2, 0),
(31240, 31239, '31241', 31233, 3, 0),
(31241, 31240, '31242', 31233, 4, 0),
(31242, 31241, NULL, 31233, 5, 0),
(31208, 0, '31209', 31208, 1, 0),
(31209, 31208, NULL, 31208, 2, 0),
(31228, 0, '31229', 31228, 1, 0),
(31229, 31228, '31230', 31228, 2, 0),
(31230, 31229, NULL, 31228, 3, 0),
(31380, 0, '31382', 31380, 1, 0),
(31382, 31380, '31383', 31380, 2, 0),
(31383, 31382, '31384', 31380, 3, 0),
(31384, 31383, '31385', 31380, 4, 0),
(31385, 31384, NULL, 31380, 5, 0),
(31569, 0, '31570', 31569, 1, 0),
(31570, 31569, NULL, 31569, 2, 0),
(31571, 0, '31572', 31571, 1, 0),
(31572, 31571, '31573', 31571, 2, 0),
(31573, 31572, NULL, 31571, 3, 0),
(31574, 0, '31575', 31574, 1, 0),
(31575, 31574, NULL, 31574, 2, 0),
(31579, 0, '31582', 31579, 1, 0),
(31582, 31579, '31583', 31579, 2, 0),
(31583, 31582, NULL, 31579, 3, 0),
(31584, 0, '31585', 31584, 1, 0),
(31585, 31584, '31586', 31584, 2, 0),
(31586, 31585, '31587', 31584, 3, 0),
(31587, 31586, '31588', 31584, 4, 0),
(31588, 31587, NULL, 31584, 5, 0),
(31638, 0, '31639', 31638, 1, 0),
(31639, 31638, '31640', 31638, 2, 0),
(31640, 31639, NULL, 31638, 3, 0),
(31641, 0, '31642', 31641, 1, 0),
(31642, 31641, NULL, 31641, 2, 0),
(31679, 0, '31680', 31679, 1, 0),
(31680, 31679, NULL, 31679, 2, 0),
(34293, 0, '34295', 34293, 1, 0),
(34295, 34293, '34296', 34293, 2, 0),
(34296, 34295, NULL, 34293, 3, 0),
(31656, 0, '31657', 31656, 1, 0),
(31657, 31656, '31658', 31656, 2, 0),
(31658, 31657, '31659', 31656, 3, 0),
(31659, 31658, '31660', 31656, 4, 0),
(31660, 31659, NULL, 31656, 5, 0),
(31667, 0, '31668', 31667, 1, 0),
(31668, 31667, '31669', 31667, 2, 0),
(31669, 31668, NULL, 31667, 3, 0),
(31670, 0, '31672', 31670, 1, 0),
(31672, 31670, NULL, 31670, 2, 0),
(31674, 0, '31675', 31674, 1, 0),
(31675, 31674, '31676', 31674, 2, 0),
(31676, 31675, '31677', 31674, 3, 0),
(31677, 31676, '31678', 31674, 4, 0),
(31678, 31677, NULL, 31674, 5, 0),
(31682, 0, '31683', 31682, 1, 0),
(31683, 31682, '31684', 31682, 2, 0),
(31684, 31683, '31685', 31682, 3, 0),
(31685, 31684, '31686', 31682, 4, 0),
(31686, 31685, NULL, 31682, 5, 0),
(31822, 0, '31823', 31822, 1, 0),
(31823, 31822, '31824', 31822, 2, 0),
(31824, 31823, NULL, 31822, 3, 0),
(31825, 0, '31826', 31825, 1, 0),
(31826, 31825, NULL, 31825, 2, 0),
(31828, 0, '31829', 31828, 1, 0),
(31829, 31828, '31830', 31828, 2, 0),
(31830, 31829, NULL, 31828, 3, 0),
(31833, 0, '31835', 31833, 1, 0),
(31835, 31833, '31836', 31833, 2, 0),
(31836, 31835, NULL, 31833, 3, 0),
(31837, 0, '31838', 31837, 1, 0),
(31838, 31837, '31839', 31837, 2, 0),
(31839, 31838, '31840', 31837, 3, 0),
(31840, 31839, '31841', 31837, 4, 0),
(31841, 31840, NULL, 31837, 5, 0),
(31844, 0, '31845', 31844, 1, 0),
(31845, 31844, NULL, 31844, 2, 0),
(31846, 0, '31847', 31846, 1, 0),
(31847, 31846, NULL, 31846, 2, 0),
(31848, 0, '31849', 31848, 1, 0),
(31849, 31848, NULL, 31848, 2, 0),
(31850, 0, '31851', 31850, 1, 0),
(31851, 31850, '31852', 31850, 2, 0),
(31852, 31851, '31853', 31850, 3, 0),
(31853, 31852, '31854', 31850, 4, 0),
(31854, 31853, NULL, 31850, 5, 0),
(31858, 0, '31859', 31858, 1, 0),
(31859, 31858, '31860', 31858, 2, 0),
(31860, 31859, '31861', 31858, 3, 0),
(31861, 31860, '31862', 31858, 4, 0),
(31862, 31861, NULL, 31858, 5, 0),
(31866, 0, '31867', 31866, 1, 0),
(31867, 31866, '31868', 31866, 2, 0),
(31868, 31867, NULL, 31866, 3, 0),
(31869, 0, '31870', 31869, 1, 0),
(31870, 31869, NULL, 31869, 2, 0),
(31871, 0, '31872', 31871, 1, 0),
(31872, 31871, '31873', 31871, 2, 0),
(31873, 31872, NULL, 31871, 3, 0),
(31876, 0, '31877', 31876, 1, 0),
(31877, 31876, '31878', 31876, 2, 0),
(31878, 31877, NULL, 31876, 3, 0),
(31879, 0, '31880', 31879, 1, 0),
(31880, 31879, '31881', 31879, 2, 0),
(31881, 31880, '31882', 31879, 3, 0),
(31882, 31881, '31883', 31879, 4, 0),
(31883, 31882, NULL, 31879, 5, 0),
(32043, 0, '35396', 32043, 1, 0),
(35396, 32043, '35397', 32043, 2, 0),
(35397, 35396, NULL, 32043, 3, 0),
(31244, 0, '31245', 31244, 1, 0),
(31245, 31244, NULL, 31244, 2, 0),
(32385, 0, '32387', 32385, 1, 0),
(32387, 32385, '32392', 32385, 2, 0),
(32392, 32387, '32393', 32385, 3, 0),
(32393, 32392, '32394', 32385, 4, 0),
(32394, 32393, NULL, 32385, 5, 0),
(32381, 0, '32382', 32381, 1, 0),
(32382, 32381, '32383', 32381, 2, 0),
(32383, 32382, NULL, 32381, 3, 0),
(33142, 0, '33145', 33142, 1, 0),
(33145, 33142, '33146', 33142, 2, 0),
(33146, 33145, NULL, 33142, 3, 0),
(33150, 0, '33154', 33150, 1, 0),
(33154, 33150, NULL, 33150, 2, 0),
(33158, 0, '33159', 33158, 1, 0),
(33159, 33158, '33160', 33158, 2, 0),
(33160, 33159, '33161', 33158, 3, 0),
(33161, 33160, '33162', 33158, 4, 0),
(33162, 33161, NULL, 33158, 5, 0),
(34753, 0, '34859', 34753, 1, 0),
(34859, 34753, '34860', 34753, 2, 0),
(34860, 34859, NULL, 34753, 3, 0),
(33167, 0, '33171', 33167, 1, 0),
(33171, 33167, '33172', 33167, 2, 0),
(33172, 33171, NULL, 33167, 3, 0),
(33174, 0, '33182', 33174, 1, 0),
(33182, 33174, NULL, 33174, 2, 0),
(33186, 0, '33190', 33186, 1, 0),
(33190, 33186, NULL, 33186, 2, 0),
(34908, 0, '34909', 34908, 1, 0),
(34909, 34908, '34910', 34908, 2, 0),
(34910, 34909, '34911', 34908, 3, 0),
(34911, 34910, '34912', 34908, 4, 0),
(34912, 34911, NULL, 34908, 5, 0),
(33201, 0, '33202', 33201, 1, 0),
(33202, 33201, '33203', 33201, 2, 0),
(33203, 33202, '33204', 33201, 3, 0),
(33204, 33203, '33205', 33201, 4, 0),
(33205, 33204, NULL, 33201, 5, 0),
(33213, 0, '33214', 33213, 1, 0),
(33214, 33213, '33215', 33213, 2, 0),
(33215, 33214, NULL, 33213, 3, 0),
(33221, 0, '33222', 33221, 1, 0),
(33222, 33221, '33223', 33221, 2, 0),
(33223, 33222, '33224', 33221, 3, 0),
(33224, 33223, '33225', 33221, 4, 0),
(33225, 33224, NULL, 33221, 5, 0),
(14910, 0, '33371', 14910, 1, 0),
(33371, 14910, NULL, 14910, 2, 0),
(33589, 0, '33590', 33589, 1, 0),
(33590, 33589, '33591', 33589, 2, 0),
(33591, 33590, NULL, 33589, 3, 0),
(33592, 0, '33596', 33592, 1, 0),
(33596, 33592, NULL, 33592, 2, 0),
(33597, 0, '33599', 33597, 1, 0),
(33599, 33597, '33956', 33597, 2, 0),
(33956, 33599, NULL, 33597, 3, 0),
(33600, 0, '33601', 33600, 1, 0),
(33601, 33600, '33602', 33600, 2, 0),
(33602, 33601, NULL, 33600, 3, 0),
(33603, 0, '33604', 33603, 1, 0),
(33604, 33603, '33605', 33603, 2, 0),
(33605, 33604, '33606', 33603, 3, 0),
(33606, 33605, '33607', 33603, 4, 0),
(33607, 33606, NULL, 33603, 5, 0),
(33879, 0, '33880', 33879, 1, 0),
(33880, 33879, NULL, 33879, 2, 0),
(33886, 0, '33887', 33886, 1, 0),
(33887, 33886, '33888', 33886, 2, 0),
(33888, 33887, '33889', 33886, 3, 0),
(33889, 33888, '33890', 33886, 4, 0),
(33890, 33889, NULL, 33886, 5, 0),
(33881, 0, '33882', 33881, 1, 0),
(33882, 33881, '33883', 33881, 2, 0),
(33883, 33882, NULL, 33881, 3, 0),
(33872, 0, '33873', 33872, 1, 0),
(33873, 33872, NULL, 33872, 2, 0),
(33851, 0, '33852', 33851, 1, 0),
(33852, 33851, '33957', 33851, 2, 0),
(33957, 33852, NULL, 33851, 3, 0),
(33853, 0, '33855', 33853, 1, 0),
(33855, 33853, '33856', 33853, 2, 0),
(33856, 33855, NULL, 33853, 3, 0),
(33859, 0, '33866', 33859, 1, 0),
(33866, 33859, '33867', 33859, 2, 0),
(33867, 33866, '33868', 33859, 3, 0),
(33868, 33867, '33869', 33859, 4, 0),
(33869, 33868, NULL, 33859, 5, 0),
(34151, 0, '34152', 34151, 1, 0),
(34152, 34151, '34153', 34151, 2, 0),
(34153, 34152, NULL, 34151, 3, 0),
(34297, 0, '34300', 34297, 1, 0),
(34300, 34297, NULL, 34297, 2, 0),
(34453, 0, '34454', 34453, 1, 0),
(34454, 34453, NULL, 34453, 2, 0),
(34455, 0, '34459', 34455, 1, 0),
(34459, 34455, '34460', 34455, 2, 0),
(34460, 34459, NULL, 34455, 3, 0),
(34462, 0, '34464', 34462, 1, 0),
(34464, 34462, '34465', 34462, 2, 0),
(34465, 34464, NULL, 34462, 3, 0),
(34466, 0, '34467', 34466, 1, 0),
(34467, 34466, '34468', 34466, 2, 0),
(34468, 34467, '34469', 34466, 3, 0),
(34469, 34468, '34470', 34466, 4, 0),
(34470, 34469, NULL, 34466, 5, 0),
(34475, 0, '34476', 34475, 1, 0),
(34476, 34475, NULL, 34475, 2, 0),
(34482, 0, '34483', 34482, 1, 0),
(34483, 34482, '34484', 34482, 2, 0),
(34484, 34483, NULL, 34482, 3, 0),
(34485, 0, '34486', 34485, 1, 0),
(34486, 34485, '34487', 34485, 2, 0),
(34487, 34486, '34488', 34485, 3, 0),
(34488, 34487, '34489', 34485, 4, 0),
(34489, 34488, NULL, 34485, 5, 0),
(34491, 0, '34492', 34491, 1, 0),
(34492, 34491, '34493', 34491, 2, 0),
(34493, 34492, NULL, 34491, 3, 0),
(34494, 0, '34496', 34494, 1, 0),
(34496, 34494, NULL, 34494, 2, 0),
(34497, 0, '34498', 34497, 1, 0),
(34498, 34497, '34499', 34497, 2, 0),
(34499, 34498, NULL, 34497, 3, 0),
(34500, 0, '34502', 34500, 1, 0),
(34502, 34500, '34503', 34500, 2, 0),
(34503, 34502, NULL, 34500, 3, 0),
(34506, 0, '34507', 34506, 1, 0),
(34507, 34506, '34508', 34506, 2, 0),
(34508, 34507, '34838', 34506, 3, 0),
(34838, 34508, '34839', 34506, 4, 0),
(34839, 34838, NULL, 34506, 5, 0),
(33191, 0, '33192', 33191, 1, 0),
(33192, 33191, '33193', 33191, 2, 0),
(33193, 33192, '33194', 33191, 3, 0),
(33194, 33193, '33195', 33191, 4, 0),
(33195, 33194, NULL, 33191, 5, 0),
(34935, 0, '34938', 34935, 1, 0),
(34938, 34935, '34939', 34935, 2, 0),
(34939, 34938, NULL, 34935, 3, 0),
(34950, 0, '34954', 34950, 1, 0),
(34954, 34950, NULL, 34950, 2, 0),
(34948, 0, '34949', 34948, 1, 0),
(34949, 34948, NULL, 34948, 2, 0),
(19498, 0, '19499', 19498, 1, 0),
(19499, 19498, '19500', 19498, 2, 0),
(19500, 19499, NULL, 19498, 3, 0),
(35104, 0, '35110', 35104, 1, 0),
(35110, 35104, '35111', 35104, 2, 0),
(35111, 35110, NULL, 35104, 3, 0),
(35363, 0, '35364', 35363, 1, 0),
(35364, 35363, NULL, 35363, 2, 0),
(35446, 0, '35448', 35446, 1, 0),
(35448, 35446, '35449', 35446, 2, 0),
(35449, 35448, '35450', 35446, 3, 0),
(35450, 35449, '35451', 35446, 4, 0),
(35451, 35450, NULL, 35446, 5, 0),
(35541, 0, '35550', 35541, 1, 0),
(35550, 35541, '35551', 35541, 2, 0),
(35551, 35550, '35552', 35541, 3, 0),
(35552, 35551, '35553', 35541, 4, 0),
(35553, 35552, NULL, 35541, 5, 0),
(35578, 0, '35581', 35578, 1, 0),
(35581, 35578, NULL, 35578, 2, 0),
(14165, 0, '14166', 14165, 1, 0),
(14166, 14165, '14167', 14165, 2, 0),
(14167, 14166, NULL, 14165, 3, 0),
(41021, 0, '41026', 41021, 1, 0),
(41026, 41021, NULL, 41021, 2, 0),
(45234, 0, '45243', 45234, 1, 0),
(45243, 45234, '45244', 45234, 2, 0),
(45244, 45243, NULL, 45234, 3, 0);

-- Fixing mangos errors with the spell chain data
-- Seal of Rigtheousness
UPDATE spell_chain SET next_spell=20287 WHERE spell_id=21084;
-- Tailoring
UPDATE spell_chain SET next_spell='26790 26797 26798 26801' WHERE spell_id=12180;
-- Blacksmith
UPDATE spell_chain SET next_spell='9787 9788 17039 17040 17041 29844' WHERE spell_id=9785;
-- Alchemy
UPDATE spell_chain SET next_spell='28596 28672 28675 28677' WHERE spell_id=11611;
-- Leatherworking
UPDATE spell_chain SET next_spell='32549 10656 10658 10660' WHERE spell_id=10662;
-- Engineering
UPDATE spell_chain SET next_spell='30350 20219 20222' WHERE spell_id=12656;
-- Herbalism
UPDATE spell_chain SET rank=2, first_spell=2366, prev_spell=2366 WHERE spell_id=2368;
UPDATE spell_chain SET rank=3, first_spell=2366 WHERE spell_id=3570;
UPDATE spell_chain SET rank=4, first_spell=2366 WHERE spell_id=11993;
UPDATE spell_chain SET rank=5, first_spell=2366 WHERE spell_id=28695;
