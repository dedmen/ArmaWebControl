params ["_name", "_code"];

private _players = allPlayers;
private _index = _players findIf {
	private _pName = name _x;

	_result = [name player, _pName] call CBA_fnc_find;
	_result != -1
};

if (_index != -1) then {
	(_players select _index) call _code;
};