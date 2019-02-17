class CfgPatches {
	class ArmaWebControl {
		name = "ArmaWebControl";
		units[] = {};
		weapons[] = {};
		requiredVersion = 1.88;
		requiredAddons[] = {"intercept_core"};
		author = "Dedmen";
		authors[] = {"Dedmen"};
		url = "https://github.com/dedmen/ArmaWebControl";
		version = "1.0";
		versionStr = "1.0";
		versionAr[] = {1,0};
	};
};
class Intercept {
    class Dedmen {
        class ArmaWebControl {
            pluginName = "ArmaWebControl";
        };
    };
};


class Extended_PreStart_EventHandlers {
    class ADDON {
        init = QUOTE(call COMPILE_FILE(XEH_preStart));
    };
};

class Extended_PreInit_EventHandlers {
    class ADDON {
        init = QUOTE(call COMPILE_FILE(XEH_preInit));
    };
};

