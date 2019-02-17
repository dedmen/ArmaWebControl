var presets_Unit = {
    makeZeus: `player call GF_fnc_makeUnitCurator`,
    makeMedic: `
        player setVariable ["ace_medical_medicClass", 5];
        player setUnitTrait ["Medic",true];`,
    giveUnitMyLoadout: `player setUnitLoadout [getUnitLoadout _this, true]`,
    makeMCCMissionMaker: `mcc_missionmaker = (name player); mcc_loginmissionmaker = false;`,
    endUnconsciousness: `player setVariable ["ACE_isUnconscious", false, true];`,
};