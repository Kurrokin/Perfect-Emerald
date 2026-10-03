// GENERATED (tools: gen_roles.py logic) from the map connections.
// SHARE: same weather as its anchor zone/city. NEIGHBOR: a different but
// compatible weather (a city's 2nd/3rd exit). OWN_AVOID: own zone weather,
// never identical to the adjacent city. COPY: same as a same-terrain route.
static const struct DynamicWeatherMapRole sDynamicWeatherMapRoles[] =
{
    {MAP_BATTLE_FRONTIER_OUTSIDE_EAST    , DWROLE_SHARE     , WSEC_FRONTIER   , WSEC_FRONTIER   , WSEC_NONE       ,  0, 0}, // 
    {MAP_BATTLE_FRONTIER_OUTSIDE_WEST    , DWROLE_SHARE     , WSEC_FRONTIER   , WSEC_FRONTIER   , WSEC_NONE       ,  0, 0}, // 
    {MAP_DEWFORD_TOWN                    , DWROLE_SHARE     , WSEC_DEWFORD    , WSEC_DEWFORD    , WSEC_NONE       ,  0, 0}, // 
    {MAP_EVER_GRANDE_CITY                , DWROLE_SHARE     , WSEC_EVER_GRANDE, WSEC_EVER_GRANDE, WSEC_NONE       ,  0, 0}, // 
    {MAP_FALLARBOR_TOWN                  , DWROLE_SHARE     , WSEC_ASHLANDS   , WSEC_ASHLANDS   , WSEC_NONE       ,  0, 0}, // 
    {MAP_FORTREE_CITY                    , DWROLE_SHARE     , WSEC_RAINFOREST , WSEC_RAINFOREST , WSEC_NONE       ,  0, 0}, // 
    {MAP_JAGGED_PASS                     , DWROLE_SHARE     , WSEC_VOLCANO    , WSEC_VOLCANO    , WSEC_NONE       ,  0, 0}, // 
    {MAP_LAVARIDGE_TOWN                  , DWROLE_SHARE     , WSEC_VOLCANO    , WSEC_VOLCANO    , WSEC_NONE       ,  0, 0}, // 
    {MAP_LILYCOVE_CITY                   , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_LITTLEROOT_TOWN                 , DWROLE_SHARE     , WSEC_LITTLEROOT , WSEC_LITTLEROOT , WSEC_NONE       ,  0, 0}, // 
    {MAP_MAUVILLE_CITY                   , DWROLE_SHARE     , WSEC_MAUVILLE   , WSEC_MAUVILLE   , WSEC_NONE       ,  0, DWFLAG_DESERT_SPILL}, // 
    {MAP_MOSSDEEP_CITY                   , DWROLE_SHARE     , WSEC_MOSSDEEP   , WSEC_MOSSDEEP   , WSEC_NONE       ,  0, 0}, // 
    {MAP_MT_PYRE_EXTERIOR                , DWROLE_SHARE     , WSEC_MT_PYRE    , WSEC_MT_PYRE    , WSEC_NONE       ,  0, 0}, // 
    {MAP_OLDALE_TOWN                     , DWROLE_SHARE     , WSEC_LITTLEROOT , WSEC_LITTLEROOT , WSEC_NONE       ,  0, 0}, // 
    {MAP_PACIFIDLOG_TOWN                 , DWROLE_SHARE     , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       ,  0, 0}, // 
    {MAP_PETALBURG_CITY                  , DWROLE_SHARE     , WSEC_PETALBURG  , WSEC_PETALBURG  , WSEC_NONE       ,  0, 0}, // 
    {MAP_PETALBURG_WOODS                 , DWROLE_SHARE     , WSEC_WOODS      , WSEC_WOODS      , WSEC_NONE       ,  0, 0}, // 
    {MAP_ROUTE101                        , DWROLE_SHARE     , WSEC_LITTLEROOT , WSEC_LITTLEROOT , WSEC_NONE       ,  0, 0}, // city MAP_LITTLEROOT_TOWN
    {MAP_ROUTE102                        , DWROLE_SHARE     , WSEC_PETALBURG  , WSEC_PETALBURG  , WSEC_NONE       ,  0, 0}, // city MAP_PETALBURG_CITY
    {MAP_ROUTE103                        , DWROLE_NEIGHBOR  , WSEC_LITTLEROOT , WSEC_LITTLEROOT , WSEC_SLATEPORT  ,  0, 0}, // city MAP_OLDALE_TOWN
    {MAP_ROUTE104                        , DWROLE_NEIGHBOR  , WSEC_PETALBURG  , WSEC_PETALBURG  , WSEC_DEWFORD    ,  0, 0}, // city MAP_PETALBURG_CITY
    {MAP_ROUTE105                        , DWROLE_COPY      , WSEC_DEWFORD    , WSEC_DEWFORD    , WSEC_NONE       , 22, 0}, // copies MAP_ROUTE106
    {MAP_ROUTE106                        , DWROLE_SHARE     , WSEC_DEWFORD    , WSEC_DEWFORD    , WSEC_NONE       ,  0, 0}, // city MAP_DEWFORD_TOWN
    {MAP_ROUTE107                        , DWROLE_NEIGHBOR  , WSEC_DEWFORD    , WSEC_DEWFORD    , WSEC_SLATEPORT  ,  0, 0}, // city MAP_DEWFORD_TOWN
    {MAP_ROUTE108                        , DWROLE_COPY      , WSEC_SLATEPORT  , WSEC_SLATEPORT  , WSEC_NONE       , 25, 0}, // copies MAP_ROUTE109
    {MAP_ROUTE109                        , DWROLE_SHARE     , WSEC_SLATEPORT  , WSEC_SLATEPORT  , WSEC_NONE       ,  0, 0}, // city MAP_SLATEPORT_CITY
    {MAP_ROUTE110                        , DWROLE_NEIGHBOR  , WSEC_SLATEPORT  , WSEC_SLATEPORT  , WSEC_LITTLEROOT ,  0, 0}, // city MAP_SLATEPORT_CITY
    {MAP_ROUTE111                        , DWROLE_OWN_AVOID , WSEC_DESERT     , WSEC_MAUVILLE   , WSEC_NONE       ,  0, 0}, // city MAP_MAUVILLE_CITY
    {MAP_ROUTE112                        , DWROLE_SHARE     , WSEC_VOLCANO    , WSEC_VOLCANO    , WSEC_NONE       ,  0, 0}, // city MAP_LAVARIDGE_TOWN
    {MAP_ROUTE113                        , DWROLE_SHARE     , WSEC_ASHLANDS   , WSEC_ASHLANDS   , WSEC_NONE       ,  0, 0}, // city MAP_FALLARBOR_TOWN
    {MAP_ROUTE114                        , DWROLE_OWN_AVOID , WSEC_METEOR     , WSEC_ASHLANDS   , WSEC_NONE       ,  0, 0}, // city MAP_FALLARBOR_TOWN
    {MAP_ROUTE115                        , DWROLE_SHARE     , WSEC_RUSTBORO   , WSEC_RUSTBORO   , WSEC_NONE       ,  0, 0}, // city MAP_RUSTBORO_CITY
    {MAP_ROUTE116                        , DWROLE_NEIGHBOR  , WSEC_RUSTBORO   , WSEC_RUSTBORO   , WSEC_MAUVILLE   ,  0, 0}, // city MAP_RUSTBORO_CITY
    {MAP_ROUTE117                        , DWROLE_SHARE     , WSEC_MAUVILLE   , WSEC_MAUVILLE   , WSEC_NONE       ,  0, 0}, // city MAP_MAUVILLE_CITY
    {MAP_ROUTE118                        , DWROLE_NEIGHBOR  , WSEC_MAUVILLE   , WSEC_MAUVILLE   , WSEC_LILYCOVE   ,  0, 0}, // city MAP_MAUVILLE_CITY
    {MAP_ROUTE119                        , DWROLE_SHARE     , WSEC_RAINFOREST , WSEC_RAINFOREST , WSEC_NONE       ,  0, 0}, // city MAP_FORTREE_CITY
    {MAP_ROUTE120                        , DWROLE_NEIGHBOR  , WSEC_RAINFOREST , WSEC_RAINFOREST , WSEC_LILYCOVE   ,  0, 0}, // city MAP_FORTREE_CITY
    {MAP_ROUTE121                        , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // city MAP_LILYCOVE_CITY
    {MAP_ROUTE122                        , DWROLE_COPY      , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       , 37, 0}, // copies MAP_ROUTE121
    {MAP_ROUTE123                        , DWROLE_COPY      , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       , 38, 0}, // copies MAP_ROUTE122
    {MAP_ROUTE124                        , DWROLE_SHARE     , WSEC_MOSSDEEP   , WSEC_MOSSDEEP   , WSEC_NONE       ,  0, 0}, // city MAP_MOSSDEEP_CITY
    {MAP_ROUTE125                        , DWROLE_NEIGHBOR  , WSEC_MOSSDEEP   , WSEC_MOSSDEEP   , WSEC_NONE       ,  0, 0}, // city MAP_MOSSDEEP_CITY
    {MAP_ROUTE126                        , DWROLE_COPY      , WSEC_SOOTOPOLIS , WSEC_SOOTOPOLIS , WSEC_NONE       , 43, 0}, // copies MAP_ROUTE127
    {MAP_ROUTE127                        , DWROLE_OWN_AVOID , WSEC_SOOTOPOLIS , WSEC_MOSSDEEP   , WSEC_NONE       ,  0, 0}, // city MAP_MOSSDEEP_CITY
    {MAP_ROUTE128                        , DWROLE_SHARE     , WSEC_SOOTOPOLIS , WSEC_EVER_GRANDE, WSEC_NONE       ,  0, 0}, // city MAP_EVER_GRANDE_CITY
    {MAP_ROUTE129                        , DWROLE_COPY      , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       , 46, 0}, // copies MAP_ROUTE130
    {MAP_ROUTE130                        , DWROLE_COPY      , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       , 47, 0}, // copies MAP_ROUTE131
    {MAP_ROUTE131                        , DWROLE_SHARE     , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       ,  0, 0}, // city MAP_PACIFIDLOG_TOWN
    {MAP_ROUTE132                        , DWROLE_NEIGHBOR  , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       ,  0, 0}, // city MAP_PACIFIDLOG_TOWN
    {MAP_ROUTE133                        , DWROLE_COPY      , WSEC_SOUTH_SEA  , WSEC_SOUTH_SEA  , WSEC_NONE       , 48, 0}, // copies MAP_ROUTE132
    {MAP_ROUTE134                        , DWROLE_OWN_AVOID , WSEC_SOUTH_SEA  , WSEC_SLATEPORT  , WSEC_NONE       ,  0, 0}, // city MAP_SLATEPORT_CITY
    {MAP_RUSTBORO_CITY                   , DWROLE_SHARE     , WSEC_RUSTBORO   , WSEC_RUSTBORO   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_NORTH               , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_NORTHEAST           , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_NORTHWEST           , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_SOUTH               , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_SOUTHEAST           , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SAFARI_ZONE_SOUTHWEST           , DWROLE_SHARE     , WSEC_LILYCOVE   , WSEC_LILYCOVE   , WSEC_NONE       ,  0, 0}, // 
    {MAP_SLATEPORT_CITY                  , DWROLE_SHARE     , WSEC_SLATEPORT  , WSEC_SLATEPORT  , WSEC_NONE       ,  0, 0}, // 
    {MAP_SOOTOPOLIS_CITY                 , DWROLE_SHARE     , WSEC_SOOTOPOLIS , WSEC_SOOTOPOLIS , WSEC_NONE       ,  0, 0}, // 
    {MAP_VERDANTURF_TOWN                 , DWROLE_SHARE     , WSEC_MAUVILLE   , WSEC_MAUVILLE   , WSEC_NONE       ,  0, 0}, // 
};
