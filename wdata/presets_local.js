var presets_Local = {
    vehicleSpeedBoost: `[(vehicle player),((vectorNormalized velocity (vehicle player)) vectorMultiply 30)] remoteExec ["setVelocity", (vehicle player)]`,
    vehicleRepair: `(vehicle player) setDamage 0;`,
    spawnFoxenal: `"wolf_foxenal_point_sit_small" createVehicle ((position player) vectorAdd [0,0,300]);`,
    teleportToMapClick: `onMapSingleClick "(vehicle player) setpos _pos; onMapSingleClick ''; true"`,
    endUnconsciousness: `player setVariable ["ACE_isUnconscious", false, true];`,
};