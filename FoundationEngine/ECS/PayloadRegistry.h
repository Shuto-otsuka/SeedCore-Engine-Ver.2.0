#pragma once
#include <FoundationEngine/Prelude.h>
#include <FoundationEngine/ECS/ReflectionRegistry.h>
#include <FoundationEngine/Utility/FlatMap.h>

namespace SeedCore
{
	/**
	* [EN]
	* Process-wide registry mapping a reflected type's name to the
	* function that enumerates the FieldInfo list of its payload
	* (asset-reference) fields specifically, used by the editor to draw
	* asset-drop targets. Populated by codegen (see
	* Tools/Python/Payload.py) from SC_PAYLOAD_FIELD* annotations.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* リフレクションされた型の名前を、その型が持つペイロード（アセット
	* 参照）フィールド専用の FieldInfo 一覧を列挙する関数へ対応付ける、
	* プロセス全体で共有されるレジストリ。エディタがアセットドロップ
	* ターゲットを描画するために使う。SC_PAYLOAD_FIELD* アノテーションから
	* コード生成（Tools/Python/Payload.py を参照）によって埋められる。
	*/
	class SEEDCORE_API PayloadRegistry
	{
	public:
		/// [EN] Function type that fills fields with the FieldInfo list describing the payload fields of the type pointed to by instance.
		/// [JP] instance が指す型のペイロードフィールドを記述する FieldInfo 一覧を fields へ書き込む、関数型。
		using PayloadFunc = std::function<void(void*, DynamicArray<FieldInfo>&)>;

		/**
		* [EN]
		* Returns the full registry mapping reflected type names to their PayloadFunc.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* リフレクションされた型名をその PayloadFunc へ対応付ける、完全な
		* レジストリを返す。
		*/
		static FlatMap<String, PayloadFunc>& GetRegistry();

		/**
		* [EN]
		* Registers function as the payload reflector for the type named name.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* name という名前の型に対するペイロードリフレクタとして function
		* を登録する。
		*/
		static void Register(String name, PayloadFunc function);
	};
}
