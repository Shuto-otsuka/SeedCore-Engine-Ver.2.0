#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	class World;
	class JobTaskflow;

	/**
	* [EN]
	* CRTP base for a system driven by a compile-time Query type: T must
	* expose a nested Query alias (a Query<Ts...> specialization) and an
	* OnExecute(World&, JobTaskflow&) member. Exposes that query's
	* archetype/read/write signatures statically, so a scheduler can
	* analyze dependencies between systems without instantiating them.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* コンパイル時の Query 型によって駆動されるシステム向けの CRTP
	* 基底クラス: T はネストされた Query エイリアス（Query<Ts...> の
	* 特殊化）と OnExecute(World&, JobTaskflow&) メンバを公開している
	* 必要がある。そのクエリのアーキタイプ/読み取り/書き込みシグネチャを
	* 静的に公開するため、スケジューラはシステムをインスタンス化せずに
	* システム間の依存関係を解析できる。
	*/
	template<typename T>
	class QuerySystem
	{
	public:
		/// [EN] The Query<Ts...> specialization this system operates over, as declared by T.
		/// [JP] T によって宣言される、このシステムが操作対象とする Query<Ts...> 特殊化。
		using QueryType = typename T::Query;

		/**
		* [EN]
		* Forwards to T::OnExecute(world, flow).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* T::OnExecute(world, flow) へ転送する。
		*/
		void Execute(World& world, JobTaskflow& flow)
		{
			static_cast<T*>(this)->OnExecute(world, flow);
		}

		/**
		* [EN]
		* Returns the combined archetype signature of QueryType.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* QueryType の合わせたアーキタイプシグネチャを返す。
		*/
		static Bitset Signature()
		{
			return QueryType::Signature();
		}

		/**
		* [EN]
		* Returns the archetype signature of only the components
		* QueryType reads.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* QueryType が読み取るコンポーネントのみのアーキタイプシグネチャを
		* 返す。
		*/
		static Bitset ReadSignature()
		{
			return QueryType::GetReadSignature();
		}

		/**
		* [EN]
		* Returns the archetype signature of only the components
		* QueryType writes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* QueryType が書き込むコンポーネントのみのアーキタイプシグネチャを
		* 返す。
		*/
		static Bitset WriteSignature()
		{
			return QueryType::GetWriteSignature();
		}
	};
}
