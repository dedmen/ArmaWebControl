params ["_name", "_code"];

private _players = allPlayers;
private _index = _players findIf {
	_result = [_name, name _x] call CBA_fnc_find;
	_result != -1
};

if (_index != -1) then {
	(_players select _index) call _code;
};