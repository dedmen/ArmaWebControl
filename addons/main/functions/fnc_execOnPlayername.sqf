params ["_name", "_code"];

{
	private _pName = name _x;

	_result = [name player, _pName] call CBA_fnc_find;
	if (_result != -1) then {
		[player, _code] remoteExec ["call", _x];
	};
} forEach allPlayers;