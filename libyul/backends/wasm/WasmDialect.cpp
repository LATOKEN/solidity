/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Dialects for Wasm.
 */

#include <libyul/backends/wasm/WasmDialect.h>

#include <libyul/AST.h>
#include <libyul/Exceptions.h>

using namespace std;
using namespace solidity::yul;

WasmDialect::WasmDialect()
{
	YulString i64 = "i64"_yulstring;
	YulString i32 = "i32"_yulstring;
	defaultType = i64;
	boolType = i32;
	types = {i64, i32};

	for (auto t: types)
		for (auto const& name: {
			"add",
			"sub",
			"mul",
			// TODO: div_s
			"div_u",
			// TODO: rem_s
			"rem_u",
			"and",
			"or",
			"xor",
			"shl",
			// TODO: shr_s
			"shr_u",
			// TODO: rotl
			// TODO: rotr
		})
			addFunction(t.str() + "." + name, {t, t}, {t});

	for (auto t: types)
		for (auto const& name: {
			"eq",
			"ne",
			// TODO: lt_s
			"lt_u",
			// TODO: gt_s
			"gt_u",
			// TODO: le_s
			"le_u",
			// TODO: ge_s
			"ge_u"
		})
			addFunction(t.str() + "." + name, {t, t}, {i32});

	addFunction("i32.eqz", {i32}, {i32});
	addFunction("i64.eqz", {i64}, {i32});

	for (auto t: types)
		for (auto const& name: {
			"clz",
			"ctz",
			"popcnt",
		})
			addFunction(t.str() + "." + name, {t}, {t});

	addFunction("i32.wrap_i64", {i64}, {i32});

	addFunction("i64.extend_i32_u", {i32}, {i64});

	addFunction("i32.store", {i32, i32}, {}, false);
	m_functions["i32.store"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i32.store"_yulstring].sideEffects.otherState = SideEffects::None;
	addFunction("i64.store", {i32, i64}, {}, false);
	// TODO: add i32.store16, i64.store8, i64.store16, i64.store32
	m_functions["i64.store"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i64.store"_yulstring].sideEffects.otherState = SideEffects::None;

	addFunction("i32.store8", {i32, i32}, {}, false);
	m_functions["i32.store8"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i32.store8"_yulstring].sideEffects.otherState = SideEffects::None;

	addFunction("i64.store8", {i32, i64}, {}, false);
	m_functions["i64.store8"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i64.store8"_yulstring].sideEffects.otherState = SideEffects::None;

	addFunction("i32.load", {i32}, {i32}, false);
	m_functions["i32.load"_yulstring].sideEffects.canBeRemoved = true;
	m_functions["i32.load"_yulstring].sideEffects.canBeRemovedIfNoMSize = true;
	m_functions["i32.load"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i32.load"_yulstring].sideEffects.memory = SideEffects::Read;
	m_functions["i32.load"_yulstring].sideEffects.otherState = SideEffects::None;
	addFunction("i64.load", {i32}, {i64}, false);
	// TODO: add i32.load8, i32.load16, i64.load8, i64.load16, i64.load32
	m_functions["i64.load"_yulstring].sideEffects.canBeRemoved = true;
	m_functions["i64.load"_yulstring].sideEffects.canBeRemovedIfNoMSize = true;
	m_functions["i64.load"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["i64.load"_yulstring].sideEffects.memory = SideEffects::Read;
	m_functions["i64.load"_yulstring].sideEffects.otherState = SideEffects::None;

	// Drop is actually overloaded for all types, but Yul does not support that.
	// Because of that, we introduce "i32.drop" and "i64.drop".
	addFunction("i32.drop", {i32}, {});
	addFunction("i64.drop", {i64}, {});

	// Select is also overloaded.
	addFunction("i32.select", {i32, i32, i32}, {i32});
	addFunction("i64.select", {i64, i64, i32}, {i64});

	addFunction("nop", {}, {});
	addFunction("unreachable", {}, {}, false);
	m_functions["unreachable"_yulstring].sideEffects.storage = SideEffects::None;
	m_functions["unreachable"_yulstring].sideEffects.memory = SideEffects::None;
	m_functions["unreachable"_yulstring].sideEffects.otherState = SideEffects::None;
	m_functions["unreachable"_yulstring].controlFlowSideEffects.canTerminate = false;
	m_functions["unreachable"_yulstring].controlFlowSideEffects.canRevert = true;
	m_functions["unreachable"_yulstring].controlFlowSideEffects.canContinue = false;

	addFunction("datasize", {i64}, {i64}, true, {LiteralKind::String});
	addFunction("dataoffset", {i64}, {i64}, true, {LiteralKind::String});

	addExternals();
}

BuiltinFunction const* WasmDialect::builtin(YulString _name) const
{
	auto it = m_functions.find(_name);
	if (it != m_functions.end())
		return &it->second;
	else
		return nullptr;
}

BuiltinFunction const* WasmDialect::discardFunction(YulString _type) const
{
	if (_type == "i32"_yulstring)
		return builtin("i32.drop"_yulstring);
	yulAssert(_type == "i64"_yulstring, "");
	return builtin("i64.drop"_yulstring);
}

BuiltinFunction const* WasmDialect::equalityFunction(YulString _type) const
{
	if (_type == "i32"_yulstring)
		return builtin("i32.eq"_yulstring);
	yulAssert(_type == "i64"_yulstring, "");
	return builtin("i64.eq"_yulstring);
}

WasmDialect const& WasmDialect::instance()
{
	static std::unique_ptr<WasmDialect> dialect;
	static YulStringRepository::ResetCallback callback{[&] { dialect.reset(); }};
	if (!dialect)
		dialect = make_unique<WasmDialect>();
	return *dialect;
}

void WasmDialect::addExternals()
{
	// These are not YulStrings because that would be too complicated with regards
	// to the YulStringRepository reset.
	static string const i64{"i64"};
	static string const i32{"i32"};
	static string const i32ptr{"i32"}; // Uses "i32" on purpose.
	struct External
	{
		string module;
		string name;
		vector<string> parameters;
		vector<string> returns;
		ControlFlowSideEffects controlFlowSideEffects = ControlFlowSideEffects{};
	};
	static vector<External> externals{
		{"env", "get_address", {i32ptr}, {}},
		{"env", "get_external_balance", {i32ptr, i32ptr}, {}},
		{"eth", "getBlockBaseFee", {i32ptr}, {}},
		{"env", "get_block_hash", {i32ptr, i32ptr}, {}},
		{"env", "invoke_contract", {i32ptr, i32, i32ptr, i32ptr, i32ptr}, {i32}},
		{"env", "copy_call_value", {i32ptr, i32ptr, i32ptr}, {}},
		{"env", "get_call_size", {}, {i32}},
		{"env", "invoke_code_contract", {i32ptr, i32, i32ptr, i32ptr, i32ptr}, {i32}},
		{"env", "invoke_delegate_contract", {i32ptr, i32, i32ptr, i32ptr, i32ptr}, {i32}},
		{"env", "invoke_static_contract", {i32ptr, i32, i32ptr, i32ptr, i32ptr}, {i32}},
		{"env", "save_storage", {i32ptr, i32ptr}, {}},
		{"env", "load_storage", {i32ptr, i32ptr}, {}},
		{"env", "get_sender", {i32ptr}, {}},
		{"env", "get_msgvalue", {i32ptr}, {}},
		{"env", "copy_code_value", {i32ptr, i32, i32}, {}},
		{"env", "get_code_size", {}, {i32}},
		{"env", "get_block_coinbase_address", {i32ptr}, {}},
		{"env", "create", {i32ptr, i32ptr, i32, i32ptr}, {i32}},
		{"env", "get_block_difficulty", {i32ptr}, {}},
		{"env", "externalCodeCopy", {i32ptr, i32ptr, i32, i32}, {}},
		{"env", "get_extcodesize", {i32ptr, i32ptr}, {}},
		{"env", "get_gas_left", {i32ptr}, {}},
		{"env", "get_block_gas_limit", {i32ptr}, {}},
		{"env", "get_tx_gas_price", {i32ptr}, {}},
		{"env", "write_log", {i32ptr, i32, i32, i32ptr, i32ptr, i32ptr, i32ptr}, {}},
		{"env", "get_block_number", {i32ptr}, {}},
		{"env", "get_tx_origin", {i32ptr}, {}},
		{"env", "set_return", {i32ptr, i32}, {}},
		{"env", "system_halt", {i32}, {}, ControlFlowSideEffects{true, true, false}},
		{"env", "get_return_size", {}, {i32}},
		{"env", "copy_return_value", {i32ptr, i32, i32}, {}},
		{"env", "crypto_keccak256", {i32ptr, i32, i32ptr}, {}},
		{"env", "selfDestruct", {i32ptr}, {}, ControlFlowSideEffects{false, true, false}},
		{"env", "get_block_timestamp", {i32ptr}, {}},
		{"debug", "print32", {i32}, {}},
		{"debug", "print64", {i64}, {}},
		{"debug", "printMem", {i32, i32}, {}},
		{"debug", "printMemHex", {i32, i32}, {}},
		{"debug", "printStorage", {i32}, {}},
		{"debug", "printStorageHex", {i32}, {}},
	};
	for (External const& ext: externals)
	{
		YulString name{ext.module + "." + ext.name};
		BuiltinFunction& f = m_functions[name];
		f.name = name;
		for (string const& p: ext.parameters)
			f.parameters.emplace_back(YulString(p));
		for (string const& p: ext.returns)
			f.returns.emplace_back(YulString(p));
		// TODO some of them are side effect free.
		f.sideEffects = SideEffects::worst();
		f.sideEffects.cannotLoop = true;
		f.sideEffects.movableApartFromEffects = !ext.controlFlowSideEffects.terminatesOrReverts();
		f.controlFlowSideEffects = ext.controlFlowSideEffects;
		f.isMSize = false;
		f.literalArguments.clear();

		static set<string> const writesToStorage{
			"save_storage",
			"invoke_contract",
			"callcode",
			"callDelegate",
			"create"
		};
		static set<string> const readsStorage{"load_storage", "callStatic"};
		if (readsStorage.count(ext.name))
			f.sideEffects.storage = SideEffects::Read;
		else if (!writesToStorage.count(ext.name))
			f.sideEffects.storage = SideEffects::None;
	}
}

void WasmDialect::addFunction(
	string _name,
	vector<YulString> _params,
	vector<YulString> _returns,
	bool _movable,
	vector<optional<LiteralKind>> _literalArguments
)
{
	YulString name{move(_name)};
	BuiltinFunction& f = m_functions[name];
	f.name = name;
	f.parameters = std::move(_params);
	yulAssert(_returns.size() <= 1, "The Wasm 1.0 specification only allows up to 1 return value.");
	f.returns = std::move(_returns);
	f.sideEffects = _movable ? SideEffects{} : SideEffects::worst();
	f.sideEffects.cannotLoop = true;
	// TODO This should be improved when LoopInvariantCodeMotion gets specialized for WASM
	f.sideEffects.movableApartFromEffects = _movable;
	f.isMSize = false;
	f.literalArguments = std::move(_literalArguments);
}
