var presets_FromUnit = {
    copyUnitLoadoutToMe: `player setUnitLoadout [getUnitLoadout _this, true]`,
    teleportMeIntoUnitsVehicle: `player moveInCargo (vehicle _this)`,
    teleportMeToUnit: `player setPos (getPos _this)`
};