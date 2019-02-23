params ["_name", "_code"];

{
	private _pName = name _x;

	_result = [name _x, _pName] call CBA_fnc_find;
	if (_result != -1) then {
		[_x, _code] remoteExec ["call", _x];
	};
} forEach allPlayers;
