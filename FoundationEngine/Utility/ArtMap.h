#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	/**
	* [EN]
	* Implementation details for ArtMap: an Adaptive Radix Tree (ART).
	* Keys are converted to a byte sequence via KeyTraits and stored in a
	* trie of byte-indexed nodes that grow/shrink between four node kinds
	* (Node4/Node16/Node48/Node256) as their child count changes,
	* keeping memory proportional to actual fan-out. Not intended to be
	* used directly outside of ArtMap.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* ArtMap の実装詳細: Adaptive Radix Tree（ART）。キーは KeyTraits
	* でバイト列に変換され、バイトでインデックスされるノードのトライ木に
	* 格納される。子の数が変化するにつれて4種類のノード
	* （Node4/Node16/Node48/Node256）の間で成長/縮小し、
	* 実際のファンアウトに比例したメモリ使用量を保つ。ArtMap 以外から
	* 直接使うことは想定していない。
	*/
	namespace Art
	{
		/**
		* [EN]
		* Converts a key K to its byte representation for ART indexing.
		* The primary template handles any trivially copyable type via a
		* raw byte copy (not order-preserving for multi-byte values);
		* specializations below handle strings and integers with
		* order-preserving encodings.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ART でのインデックス付けのため、キー K をバイト表現に変換する。
		* プライマリテンプレートは、trivially copyable な任意の型を生バイト
		* コピーで扱う（複数バイト値の順序は保持されない）。以下の特殊化は
		* 文字列と整数を順序保存エンコーディングで扱う。
		*/
		template <typename K, typename = void>
		struct KeyTraits
		{
			static_assert(std::is_trivially_copyable_v<K>, "KeyTraits is not specialized for this type, and it is not trivially copyable.");

			static DynamicArray<Uint8> to_bytes(const K& key)
			{
				DynamicArray<Uint8> _out(sizeof(K));
				std::memcpy(_out.data(), &key, sizeof(K));
				return _out;
			}
		};

		/**
		* [EN]
		* KeyTraits for String: UTF-8 bytes plus a trailing NUL
		* (so no encoded key is ever a byte-prefix of another).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* String 用の KeyTraits: UTF-8 バイト列と末尾のNUL
		* （これにより、あるエンコード済みキーが別のキーの
		* バイトプレフィックスになることがなくなる）。
		*/
		template <>
		struct KeyTraits<String>
		{
			static DynamicArray<Uint8> to_bytes(String str)
			{
				DynamicArray<Uint8> _out(str.str().size() + 1);
				std::memcpy(_out.data(), str.str().data(), str.str().size());
				_out.back() = 0;
				return _out;
			}
		};

		/**
		* [EN]
		* KeyTraits for std::string, same NUL-terminated encoding as String.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::string 用の KeyTraits。String と同じNUL終端エンコーディング。
		*/
		template <>
		struct KeyTraits<std::string>
		{
			static DynamicArray<Uint8> to_bytes(const std::string& str)
			{
				DynamicArray<Uint8> _out(str.size() + 1);
				std::memcpy(_out.data(), str.data(), str.size());
				_out.back() = 0;
				return _out;
			}
		};

		/**
		* [EN]
		* KeyTraits for std::string_view, same NUL-terminated encoding as String.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* std::string_view 用の KeyTraits。String と同じNUL終端エンコーディング。
		*/
		template <>
		struct KeyTraits<std::string_view>
		{
			static DynamicArray<Uint8> to_bytes(std::string_view str)
			{
				DynamicArray<Uint8> _out(str.size() + 1);
				std::memcpy(_out.data(), str.data(), str.size());
				_out.back() = 0;
				return _out;
			}
		};

		/**
		* [EN]
		* KeyTraits for a pre-encoded byte key: passed through unchanged.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンコード済みバイト列キー用の KeyTraits: そのまま返す。
		*/
		template <>
		struct KeyTraits<DynamicArray<Uint8>>
		{
			static DynamicArray<Uint8> to_bytes(const DynamicArray<Uint8>& key)
			{
				return key;
			}
		};

		/**
		* [EN]
		* KeyTraits for unsigned integers: big-endian bytes, so
		* lexicographic byte order matches numeric order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 符号なし整数用の KeyTraits: ビッグエンディアンのバイト列にする
		* ことで、辞書式バイト順が数値順と一致するようにする。
		*/
		template <std::unsigned_integral U>
		struct KeyTraits<U>
		{
			static DynamicArray<Uint8> to_bytes(U key)
			{
				DynamicArray<Uint8> _out(sizeof(U));
				for (Size index = sizeof(U); index-- > 0;)
				{
					_out[sizeof(U) - 1 - index] = static_cast<Uint8>((key >> (index * 8)) & 0xFF);
				}
				return _out;
			}
		};

		/**
		* [EN]
		* KeyTraits for signed integers: flips the sign bit before
		* encoding as unsigned, so negative values sort before
		* non-negative ones in byte order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 符号付き整数用の KeyTraits: 符号なしとしてエンコードする前に
		* 符号ビットを反転することで、負の値がバイト順で非負の値より
		* 前にくるようにする。
		*/
		template <std::signed_integral S>
		struct KeyTraits<S>
		{
			static DynamicArray<Uint8> to_bytes(S key)
			{
				using U = std::make_unsigned_t<S>;
				U u = static_cast<U>(key) ^ (U(1) << (sizeof(U) * 8 - 1));
				return KeyTraits<U>::to_bytes(u);
			}
		};

		/// [EN] Maximum inline path-compressed prefix length stored per node.
		/// [JP] ノードごとに格納される、パス圧縮された接頭辞のインライン最大長。
		static constexpr Size MAX_PREFIX = 8;

		/// [EN] Discriminates the concrete type behind a NodePtr.
		/// [JP] NodePtr の背後にある実際の型を判別する。
		enum class NodeType : Uint8
		{
			Node4,
			Node16,
			Node48,
			Node256,
			Leaf
		};

		/**
		* [EN]
		* Common header shared by every inner node type (and inherited,
		* unused, by Leaf), holding the path-compressed prefix and
		* child count.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全ての内部ノード型に共通するヘッダ（Leaf も継承するが未使用）。
		* パス圧縮された接頭辞と子の数を保持する。
		*/
		struct NodeBase
		{
			explicit NodeBase(NodeType type) : type_(type)
			{
				/// No Code
			}

			virtual ~NodeBase() = default;

			/**
			* [EN]
			* Returns how many bytes of this node's stored prefix match
			* key starting at depth (up to MAX_PREFIX).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* このノードに格納された接頭辞のうち、depth から始まる key と
			* 一致するバイト数を返す（MAX_PREFIX まで）。
			*/
			Uint32 check_prefix(std::span<const Uint8> key, Uint32 depth)const noexcept
			{
				Uint32 match = 0;
				const Uint32 limit = static_cast<Uint32>(Min<Uint32>(prefixLength_, MAX_PREFIX));
				for (; match < limit && depth + match < key.size(); ++match)
				{
					if (prefix_[match] != key[depth + match])
					{
						break;
					}
				}
				return match;
			}

			/// [EN] Concrete node kind, discriminating the pointee behind a NodePtr.
			/// [JP] 具体的なノード種別。NodePtr の背後にある実体を判別する。
			NodeType type_;

			/// [EN] Number of valid bytes in prefix_ (may exceed MAX_PREFIX
			///      logically; only the first MAX_PREFIX are stored inline).
			/// [JP] prefix_ の有効バイト数（論理的には MAX_PREFIX を
			///      超えることがあるが、インラインには先頭 MAX_PREFIX のみ格納する）。
			Uint8 prefixLength_ = 0;

			/// [EN] Inline path-compressed key-byte prefix shared by all descendants.
			/// [JP] 全子孫が共有する、パス圧縮されたインラインのキーバイト接頭辞。
			Uint8 prefix_[MAX_PREFIX];

			/// [EN] Number of children currently stored in this node.
			/// [JP] このノードに現在格納されている子の数。
			Uint16 numberChildren_ = 0;
		};

		/**
		* [EN]
		* Terminal node holding one key/value pair.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 1つのキー/値ペアを保持する末端ノード。
		*/
		template <typename K, typename V>
		struct Leaf final : NodeBase
		{
			Leaf(K key, V value) : NodeBase(NodeType::Leaf), pair_(std::move(key), std::move(value))
			{
				/// No Code
			}

			/**
			* [EN]
			* Returns whether this leaf's encoded key equals key.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* このリーフのエンコード済みキーが key と等しいかを返す。
			*/
			template <typename Traits>
			Bool matches(std::span<const Uint8> key)const noexcept
			{
				return std::ranges::equal(Traits::to_bytes(pair_.first), key);
			}

			/// [EN] The stored key/value pair.
			/// [JP] 格納されているキー/値ペア。
			std::pair<const K, V> pair_;
		};

		struct Node4;
		struct Node16;
		struct Node48;
		struct Node256;

		/// [EN] Non-owning pointer to any NodeBase-derived node (inner node or Leaf).
		/// [JP] NodeBase 派生ノード（内部ノードまたは Leaf）への非所有ポインタ。
		using NodePtr = NodeBase*;

		/**
		* [EN]
		* Inner node holding up to 4 children in parallel sorted arrays
		* (linear scan for lookup; smallest/cheapest node kind).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最大4個の子を、並列なソート済み配列に保持する内部ノード
		* （検索は線形走査; 最も小さく安価なノード種別）。
		*/
		struct Node4 final : NodeBase
		{
			Node4() : NodeBase(NodeType::Node4)
			{
				/// No Code
			}

			/**
			* [EN]
			* Returns a pointer to the child slot for byte, or nullptr if absent.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte に対応する子スロットへのポインタを返す。存在しなければ nullptr。
			*/
			NodePtr* find(Uint8 byte)noexcept
			{
				for (Uint16 index = 0; index < numberChildren_; ++index)
				{
					if (keys_[index] == byte)
					{
						return &children_[index];
					}
				}
				return nullptr;
			}

			/**
			* [EN]
			* Inserts child keyed by byte, keeping keys_ sorted (linear shift).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとして child を挿入する。keys_ のソート順は
			*      線形シフトにより維持される。
			*/
			void add(Uint8 byte, NodePtr child)
			{
				Uint8 index = static_cast<Uint8>(numberChildren_);
				while (index > 0 && keys_[index - 1] > byte)
				{
					keys_[index] = keys_[index - 1];
					children_[index] = children_[index - 1];
					--index;
				}
				keys_[index] = byte;
				children_[index] = child;
				++numberChildren_;
			}

			/**
			* [EN]
			* Removes the child keyed by byte, if present, compacting the arrays.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとする子が存在すれば削除し、配列を詰める。
			*/
			void remove(Uint8 byte)
			{
				for (Uint16 index = 0; index < numberChildren_; ++index)
				{
					if (keys_[index] == byte)
					{
						std::copy(keys_.begin() + index + 1, keys_.begin() + numberChildren_, keys_.begin() + index);
						std::copy(children_.begin() + index + 1, children_.begin() + numberChildren_, children_.begin() + index);
						--numberChildren_;
						return;
					}
				}
			}

			/**
			* [EN]
			* Returns the child with the smallest key byte, or nullptr if empty.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 最小のキーバイトを持つ子を返す。空なら nullptr。
			*/
			NodePtr first()noexcept
			{
				return numberChildren_ ? children_[0] : nullptr;
			}

			/**
			* [EN]
			* Returns the child with the largest key byte, or nullptr if empty.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 最大のキーバイトを持つ子を返す。空なら nullptr。
			*/
			NodePtr last()noexcept
			{
				return numberChildren_ ? children_[numberChildren_ - 1] : nullptr;
			}

			/// [EN] Sorted key bytes, parallel to children_.
			/// [JP] children_ と対をなす、ソート済みのキーバイト列。
			StaticArray<Uint8, 4> keys_;

			/// [EN] Child pointers, parallel to keys_.
			/// [JP] keys_ と対をなす子ポインタ列。
			StaticArray<NodePtr, 4> children_;
		};

		/**
		* [EN]
		* Inner node holding up to 16 children, same shape as Node4
		* (sorted parallel arrays, linear-scan lookup) at a larger capacity.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最大16個の子を保持する内部ノード。Node4 と同じ構造
		* (ソート済み並列配列、線形走査検索) で容量のみ大きい。
		*/
		struct Node16 final : NodeBase
		{
			Node16() : NodeBase(NodeType::Node16)
			{
				/// No Code
			}

			/**
			* [EN]
			* Returns a pointer to the child slot for byte, or nullptr if absent.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte に対応する子スロットへのポインタを返す。存在しなければ nullptr。
			*/
			NodePtr* find(Uint8 byte)noexcept
			{
				for (Uint16 index = 0; index < numberChildren_; ++index)
				{
					if (keys_[index] == byte)
					{
						return &children_[index];
					}
				}
				return nullptr;
			}

			/**
			* [EN]
			* Inserts child keyed by byte, keeping keys_ sorted (linear shift).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとして child を挿入する。keys_ のソート順は
			*      線形シフトにより維持される。
			*/
			void add(Uint8 byte, NodePtr child)
			{
				Uint16 index = numberChildren_;
				while (index > 0 && keys_[index - 1] > byte)
				{
					keys_[index] = keys_[index - 1];
					children_[index] = children_[index - 1];
					--index;
				}
				keys_[index] = byte;
				children_[index] = child;
				++numberChildren_;
			}

			/**
			* [EN]
			* Removes the child keyed by byte, if present, compacting the arrays.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとする子が存在すれば削除し、配列を詰める。
			*/
			void remove(Uint8 byte)
			{
				for (Uint16 index = 0; index < numberChildren_; ++index)
				{
					if (keys_[index] == byte)
					{
						std::copy(keys_.begin() + index + 1, keys_.begin() + numberChildren_, keys_.begin() + index);
						std::copy(children_.begin() + index + 1, children_.begin() + numberChildren_, children_.begin() + index);
						--numberChildren_;
						return;
					}
				}
			}

			/// [EN] Sorted key bytes, parallel to children_.
			/// [JP] children_ と対をなす、ソート済みのキーバイト列。
			StaticArray<Uint8, 16> keys_;

			/// [EN] Child pointers, parallel to keys_.
			/// [JP] keys_ と対をなす子ポインタ列。
			StaticArray<NodePtr, 16> children_;
		};

		/**
		* [EN]
		* Inner node holding up to 48 children. Trades a 256-entry
		* byte-to-slot index for O(1) lookup while keeping
		* children_ compact (only 48 slots) by reusing freed slots.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最大48個の子を保持する内部ノード。256エントリの
		* バイト→スロットのインデックスを使うことで O(1) 検索を
		* 実現しつつ、解放されたスロットを再利用して children_ を
		* コンパクト（48スロットのみ）に保つ。
		*/
		struct Node48 final : NodeBase
		{
			Node48() : NodeBase(NodeType::Node48)
			{
				index_.fill(0xFF);
			}

			/**
			* [EN]
			* Returns a pointer to the child slot for byte, or nullptr if absent.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte に対応する子スロットへのポインタを返す。存在しなければ nullptr。
			*/
			NodePtr* find(Uint8 byte)noexcept
			{
				if (index_[byte] == 0xFF)
				{
					return nullptr;
				}
				return &children_[index_[byte]];
			}

			/**
			* [EN]
			* Inserts child keyed by byte into the first free children_ slot.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとして child を、children_ の最初の空きスロットに挿入する。
			*/
			void add(Uint8 byte, NodePtr child)
			{
				Uint8 slot = 0;
				while (children_[slot] != nullptr)
				{
					++slot;
				}
				index_[byte] = slot;
				children_[slot] = child;
				++numberChildren_;
			}

			/**
			* [EN]
			* Removes the child keyed by byte, freeing its slot.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとする子を削除し、そのスロットを解放する。
			*/
			void remove(Uint8 byte)
			{
				Uint8 slot = index_[byte];
				children_[slot] = nullptr;
				index_[byte] = 0xFF;
				--numberChildren_;
			}

			/// [EN] Maps a key byte to its slot in children_, or 0xFF if unused.
			/// [JP] キーバイトを children_ 内のスロットへ写像する。未使用なら 0xFF。
			StaticArray<Uint8, 256> index_;

			/// [EN] Child pointers, sparsely populated and indexed via index_.
			/// [JP] index_ 経由でアクセスされる、疎に埋まった子ポインタ列。
			StaticArray<NodePtr, 48>  children_;
		};

		/**
		* [EN]
		* Inner node holding up to 256 children, directly indexed by
		* key byte (no separate index table needed).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最大256個の子を保持する内部ノード。キーバイトで直接
		* インデックスされる（別途インデックステーブルは不要）。
		*/
		struct Node256 final : NodeBase
		{
			Node256() : NodeBase(NodeType::Node256)
			{
				/// No Code
			}

			/**
			* [EN]
			* Returns a pointer to the child slot for byte, or nullptr if absent.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte に対応する子スロットへのポインタを返す。存在しなければ nullptr。
			*/
			NodePtr* find(Uint8 byte)noexcept
			{
				if (!children_[byte])
				{
					return nullptr;
				}
				return &children_[byte];
			}

			/**
			* [EN]
			* Sets the child keyed by byte directly (index == key byte).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとする子を直接設定する（インデックス == キーバイト）。
			*/
			void add(Uint8 byte, NodePtr child)
			{
				children_[byte] = child;
				++numberChildren_;
			}

			/**
			* [EN]
			* Clears the child keyed by byte.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* byte をキーとする子をクリアする。
			*/
			void remove(Uint8 byte)
			{
				children_[byte] = nullptr;
				--numberChildren_;
			}

			/// [EN] Child pointers, directly indexed by key byte (nullptr = absent).
			/// [JP] キーバイトで直接インデックスされる子ポインタ列（nullptr = 不在）。
			StaticArray<NodePtr, 256> children_;
		};

		/**
		* [EN]
		* Promotes a Node4 to a Node16 (called once its 4 slots fill up),
		* copying the prefix and all children/keys.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node4 を Node16 に昇格する（4スロットが埋まった時点で呼ばれる）。
		* 接頭辞と全ての子/キーをコピーする。
		*/
		inline Node16* grow(Node4* node)
		{
			auto* node16 = new Node16();
			node16->prefixLength_ = node->prefixLength_;
			node16->numberChildren_ = node->numberChildren_;
			std::memcpy(node16->prefix_, node->prefix_, node->prefixLength_);
			std::copy(node->keys_.begin(), node->keys_.begin() + node->numberChildren_, node16->keys_.begin());
			std::copy(node->children_.begin(), node->children_.begin() + node->numberChildren_, node16->children_.begin());
			return node16;
		}

		/**
		* [EN]
		* Promotes a Node16 to a Node48 (called once its 16 slots fill
		* up), copying the prefix and re-inserting all children via add.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node16 を Node48 に昇格する（16スロットが埋まった時点で
		* 呼ばれる）。接頭辞をコピーし、全ての子を add 経由で再挿入する。
		*/
		inline Node48* grow(Node16* node)
		{
			auto* node48 = new Node48();
			node48->prefixLength_ = node->prefixLength_;
			node48->numberChildren_ = node->numberChildren_;
			std::memcpy(node48->prefix_, node->prefix_, node->prefixLength_);
			for (Uint16 index = 0; index < node->numberChildren_; ++index)
			{
				node48->add(node->keys_[index], node->children_[index]);
			}
			node48->numberChildren_ = node->numberChildren_;
			return node48;
		}

		/**
		* [EN]
		* Promotes a Node48 to a Node256 (called once its 48 slots fill
		* up), copying the prefix and directly placing each child by its key byte.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node48 を Node256 に昇格する（48スロットが埋まった時点で
		* 呼ばれる）。接頭辞をコピーし、各子をそのキーバイトの位置に直接配置する。
		*/
		inline Node256* grow(Node48* node)
		{
			auto* node256 = new Node256();
			node256->prefixLength_ = node->prefixLength_;
			node256->numberChildren_ = node->numberChildren_;
			std::memcpy(node256->prefix_, node->prefix_, node->prefixLength_);
			for (Int b = 0; b < 256; ++b)
			{
				if (node->index_[b] != 0xFF)
				{
					node256->children_[b] = node->children_[node->index_[b]];
				}
			}
			return node256;
		}

		/**
		* [EN]
		* Demotes a Node16 to a Node4 (called once its child count
		* drops low enough), copying the prefix and all children/keys.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node16 を Node4 に降格する（子の数が十分少なくなった時点で
		* 呼ばれる）。接頭辞と全ての子/キーをコピーする。
		*/
		inline Node4* shrink(Node16* node)
		{
			auto* node4 = new Node4();
			node4->prefixLength_ = node->prefixLength_;
			node4->numberChildren_ = node->numberChildren_;
			std::memcpy(node4->prefix_, node->prefix_, node->prefixLength_);
			std::copy(node->keys_.begin(), node->keys_.begin() + node->numberChildren_, node4->keys_.begin());
			std::copy(node->children_.begin(), node->children_.begin() + node->numberChildren_, node4->children_.begin());
			return node4;
		}

		/**
		* [EN]
		* Demotes a Node48 to a Node16 (called once its child count
		* drops low enough), copying the prefix and re-inserting all children via add.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node48 を Node16 に降格する（子の数が十分少なくなった時点で
		* 呼ばれる）。接頭辞をコピーし、全ての子を add 経由で再挿入する。
		*/
		inline Node16* shrink(Node48* node)
		{
			auto* node16 = new Node16();
			node16->prefixLength_ = node->prefixLength_;
			std::memcpy(node16->prefix_, node->prefix_, node->prefixLength_);
			for (Int b = 0; b < 256; ++b)
			{
				if (node->index_[b] != 0xFF)
				{
					node16->add(static_cast<Uint8>(b), node->children_[node->index_[b]]);
				}
			}
			return node16;
		}

		/**
		* [EN]
		* Demotes a Node256 to a Node48 (called once its child count
		* drops low enough), copying the prefix and re-inserting all children via add.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Node256 を Node48 に降格する（子の数が十分少なくなった時点で
		* 呼ばれる）。接頭辞をコピーし、全ての子を add 経由で再挿入する。
		*/
		inline Node48* shrink(Node256* node)
		{
			auto* node48 = new Node48();
			node48->prefixLength_ = node->prefixLength_;
			std::memcpy(node48->prefix_, node->prefix_, node->prefixLength_);
			for (Int b = 0; b < 256; ++b)
			{
				if (node->children_[b])
				{
					node48->add(static_cast<Uint8>(b), node->children_[b]);
				}
			}
			return node48;
		}

		/**
		* [EN]
		* Casts a NodePtr known to be a Leaf<K, V> to its concrete type.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* Leaf<K, V> であることが分かっている NodePtr を、具体的な型にキャストする。
		*/
		template <typename K, typename V>
		Leaf<K, V>* as_leaf(NodePtr ptr)
		{
			return static_cast<Leaf<K, V>*>(ptr);
		}

		/**
		* [EN]
		* Dispatches to the concrete inner node type's find, looking up
		* the child slot for byte.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 具体的な内部ノード型の find にディスパッチし、byte に対応する
		* 子スロットを検索する。
		*/
		inline NodePtr* find(NodePtr node, Uint8 byte)
		{
			switch (node->type_)
			{
			case NodeType::Node4:
				return static_cast<Node4*>(node)->find(byte);
			case NodeType::Node16:
				return static_cast<Node16*>(node)->find(byte);
			case NodeType::Node48:
				return static_cast<Node48*>(node)->find(byte);
			case NodeType::Node256:
				return static_cast<Node256*>(node)->find(byte);
			default:
				return nullptr;
			}
		}

		/**
		* [EN]
		* Adds child keyed by byte to the concrete inner node behind
		* nodeRef, promoting it to the next larger node kind (via grow)
		* first if it is already at capacity. May replace nodeRef in place.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nodeRef の背後にある具体的な内部ノードに、byte をキーとして
		* child を追加する。既に容量いっぱいであれば、先に（grow 経由で）
		* 次の大きいノード種別に昇格する。nodeRef はその場で置き換えられ得る。
		*/
		inline void add(NodePtr& nodeRef, Uint8 byte, NodePtr child)
		{
			switch (nodeRef->type_)
			{
			case NodeType::Node4:
			{
				auto* node4 = static_cast<Node4*>(nodeRef);
				if (node4->numberChildren_ < 4)
				{
					node4->add(byte, child);
					return;
				}
				auto* node16 = grow(node4);
				delete node4;
				nodeRef = node16;
				node16->add(byte, child);
				return;
			}
			case NodeType::Node16:
			{
				auto* node16 = static_cast<Node16*>(nodeRef);
				if (node16->numberChildren_ < 16)
				{
					node16->add(byte, child);
					return;
				}
				auto* node48 = grow(node16);
				delete node16;
				nodeRef = node48;
				node48->add(byte, child);
				return;
			}
			case NodeType::Node48:
			{
				auto* node48 = static_cast<Node48*>(nodeRef);
				if (node48->numberChildren_ < 48)
				{
					node48->add(byte, child);
					return;
				}
				auto* node256 = grow(node48);
				delete node48;
				nodeRef = node256;
				node256->add(byte, child);
				return;
			}
			case NodeType::Node256:
				static_cast<Node256*>(nodeRef)->add(byte, child);
				return;
			default:
				break;
			}
		}

		/**
		* [EN]
		* Removes the child keyed by byte from the concrete inner node
		* behind nodeRef, demoting it to the next smaller node kind (via
		* shrink) if its child count drops below that kind's threshold.
		* May replace nodeRef in place.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* nodeRef の背後にある具体的な内部ノードから、byte をキーとする
		* 子を削除する。子の数がその種別のしきい値を下回った場合、
		* （shrink 経由で）次の小さいノード種別に降格する。nodeRef は
		* その場で置き換えられ得る。
		*/
		inline void remove(NodePtr& nodeRef, Uint8 byte)
		{
			switch (nodeRef->type_)
			{
			case NodeType::Node4:
				static_cast<Node4*>(nodeRef)->remove(byte);
				break;
			case NodeType::Node16:
			{
				auto* node16 = static_cast<Node16*>(nodeRef);
				node16->remove(byte);
				if (node16->numberChildren_ <= 3)
				{
					auto* node4 = shrink(node16);
					delete node16;
					nodeRef = node4;
				}
				break;
			}
			case NodeType::Node48:
			{
				auto* node48 = static_cast<Node48*>(nodeRef);
				node48->remove(byte);
				if (node48->numberChildren_ <= 12)
				{
					auto* node16 = shrink(node48);
					delete node48;
					nodeRef = node16;
				}
				break;
			}
			case NodeType::Node256:
			{
				auto* node256 = static_cast<Node256*>(nodeRef);
				node256->remove(byte);
				if (node256->numberChildren_ <= 37)
				{
					auto* node48 = shrink(node256);
					delete node256;
					nodeRef = node48;
				}
				break;
			}
			default:
				break;
			}
		}

		/**
		* [EN]
		* Descends via each node's smallest-key child until reaching a
		* Leaf, returning the leaf with the lexicographically smallest
		* key under node (or nullptr if node is null).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 各ノードの最小キーの子を辿って Leaf に到達し、node 以下で
		* 辞書式に最小のキーを持つリーフを返す（node が null なら
		* nullptr）。
		*/
		template <typename K, typename V>
		Leaf<K, V>* minimum(NodePtr node)
		{
			if (!node)
			{
				return nullptr;
			}

			if (node->type_ == NodeType::Leaf)
			{
				return as_leaf<K, V>(node);
			}

			switch (node->type_)
			{
			case NodeType::Node4:
				return minimum<K, V>(static_cast<Node4*>(node)->first());
			case NodeType::Node16:
				return minimum<K, V>(static_cast<Node16*>(node)->children_[0]);
			case NodeType::Node48:
			{
				auto* node48 = static_cast<Node48*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					if (node48->index_[b] != 0xFF)
					{
						return minimum<K, V>(node48->children_[node48->index_[b]]);
					}
				}
				return nullptr;
			}
			case NodeType::Node256:
			{
				auto* node256 = static_cast<Node256*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					if (node256->children_[b])
					{
						return minimum<K, V>(node256->children_[b]);
					}
				}
				return nullptr;
			}
			default:
				return nullptr;
			}
		}

		/**
		* [EN]
		* Recursively deletes node and its entire subtree (all inner
		* nodes and leaves). No-op for a null node.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node とそのサブツリー全体（全ての内部ノードとリーフ）を
		* 再帰的に削除する。node が null の場合は何もしない。
		*/
		template <typename K, typename V>
		void destroy(NodePtr node)
		{
			if (!node)
			{
				return;
			}

			if (node->type_ == NodeType::Leaf)
			{
				delete as_leaf<K, V>(node);
				return;
			}

			switch (node->type_)
			{
			case NodeType::Node4:
			{
				auto* node4 = static_cast<Node4*>(node);
				for (Uint16 index = 0; index < node4->numberChildren_; ++index)
				{
					destroy<K, V>(node4->children_[index]);
				}
				delete node4;
				break;
			}
			case NodeType::Node16:
			{
				auto* node16 = static_cast<Node16*>(node);
				for (Uint16 index = 0; index < node16->numberChildren_; ++index)
				{
					destroy<K, V>(node16->children_[index]);
				}
				delete node16;
				break;
			}
			case NodeType::Node48:
			{
				auto* node48 = static_cast<Node48*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					if (node48->index_[b] != 0xFF)
					{
						destroy<K, V>(node48->children_[node48->index_[b]]);
					}
				}
				delete node48;
				break;
			}
			case NodeType::Node256:
			{
				auto* node256 = static_cast<Node256*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					destroy<K, V>(node256->children_[b]);
				}
				delete node256;
				break;
			}
			default:
				break;
			}
		}

		/**
		* [EN]
		* Recursively visits every Leaf under node in key order,
		* invoking function(leaf) for each one.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* node 以下の全ての Leaf をキー順に再帰的に訪問し、それぞれに
		* 対して function(leaf) を呼び出す。
		*/
		template <typename K, typename V, typename F>
		void traverse(NodePtr node, F&& function)
		{
			if (!node)
			{
				return;
			}

			if (node->type_ == NodeType::Leaf)
			{
				function(*as_leaf<K, V>(node));
				return;
			}

			switch (node->type_)
			{
			case NodeType::Node4:
			{
				auto* node4 = static_cast<Node4*>(node);
				for (Uint16 index = 0; index < node4->numberChildren_; ++index)
				{
					traverse<K, V>(node4->children_[index], function);
				}
				break;
			}
			case NodeType::Node16:
			{
				auto* node16 = static_cast<Node16*>(node);
				for (Uint16 index = 0; index < node16->numberChildren_; ++index)
				{
					traverse<K, V>(node16->children_[index], function);
				}
				break;
			}
			case NodeType::Node48:
			{
				auto* node48 = static_cast<Node48*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					if (node48->index_[b] != 0xFF)
					{
						traverse<K, V>(node48->children_[node48->index_[b]], function);
					}
				}
				break;
			}
			case NodeType::Node256:
			{
				auto* node256 = static_cast<Node256*>(node);
				for (Int b = 0; b < 256; ++b)
				{
					traverse<K, V>(node256->children_[b], function);
				}
				break;
			}
			default:
				break;
			}
		}

		/**
		* [EN]
		* One frame of iterative in-order traversal state: a node being
		* visited plus the index of the next child to explore.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 反復的なイン・オーダー走査状態の1フレーム。訪問中のノードと、
		* 次に探索する子のインデックスを保持する。
		*/
		struct StackFrame
		{
			/// [EN] Node currently being visited.
			/// [JP] 現在訪問中のノード。
			NodePtr node_ = nullptr;

			/// [EN] Index of the next child of node_ to explore.
			/// [JP] node_ の次に探索すべき子のインデックス。
			Int cursor_ = 0;
		};

		/**
		* [EN]
		* Advances an explicit-stack, iterative in-order traversal
		* (ArtMap::Iterator/ConstIterator's underlying mechanism):
		* pushes deeper into the tree from the top frame until a Leaf is
		* reached, popping exhausted frames along the way. Returns the next
		* leaf in key order, or nullptr once the stack is empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 明示的スタックによる反復的なイン・オーダー走査を1歩進める
		* （ArtMap::Iterator/ConstIterator の内部機構）。Leaf に
		* 到達するまでトップのフレームから木を深く push し、途中で
		* 使い切ったフレームを pop する。キー順で次のリーフを返す。
		* スタックが空になれば nullptr。
		*/
		template <typename K, typename V>
		Leaf<K, V>* descend(DynamicArray<StackFrame>& stack)
		{
			while (!stack.empty())
			{
				auto& [node, cursor] = stack.back();

				if (node->type_ == NodeType::Leaf)
				{
					return as_leaf<K, V>(node);
				}

				NodePtr next = nullptr;

				switch (node->type_)
				{
				case NodeType::Node4:
				{
					auto* node4 = static_cast<Node4*>(node);
					if (cursor < node4->numberChildren_)
					{
						next = node4->children_[cursor++];
					}
					break;
				}
				case NodeType::Node16:
				{
					auto* node16 = static_cast<Node16*>(node);
					if (cursor < node16->numberChildren_)
					{
						next = node16->children_[cursor++];
					}
					break;
				}
				case NodeType::Node48:
				{
					auto* node48 = static_cast<Node48*>(node);
					while (cursor < 256 && node48->index_[cursor] == 0xFF)
					{
						++cursor;
					}
					if (cursor < 256)
					{
						next = node48->children_[node48->index_[cursor++]];
					}
					break;
				}
				case NodeType::Node256:
				{
					auto* node256 = static_cast<Node256*>(node);
					while (cursor < 256 && !node256->children_[cursor])
					{
						++cursor;
					}
					if (cursor < 256)
					{
						next = node256->children_[cursor++];
					}
					break;
				}
				default:
					break;
				}

				if (next)
				{
					stack.push_back({ next, 0 });
				}
				else
				{
					stack.pop_back();
				}
			}
			return nullptr;
		}
	}
}

namespace SeedCore
{
	/**
	* [EN]
	* Ordered associative container (key/value map) backed by an Adaptive
	* Radix Tree (see the Art namespace). Compared to a hash map, keys
	* are stored/iterated in sorted byte order and support efficient
	* prefix_search; Traits (defaulting to Art::KeyTraits<K>)
	* controls how K is encoded to bytes for indexing.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* Adaptive Radix Tree（Art 名前空間を参照）を裏付けとする、順序付き
	* 連想コンテナ（キー/値マップ）。ハッシュマップと比べ、キーはソート
	* されたバイト順で格納/反復され、効率的な prefix_search をサポート
	* する。Traits（既定は Art::KeyTraits<K>）が、インデックス付けの
	* ために K をどうバイトへエンコードするかを制御する。
	*/
	template <typename K, typename V, typename Traits = Art::KeyTraits<K>>
	class ArtMap
	{
	private:
		using Leaf = Art::Leaf<K, V>;
		using NodePtr = Art::NodePtr;

		using KeyType = DynamicArray<Uint8>;
		using MappedType = V;
		using ValueType = std::pair<const KeyType, MappedType>;

	public:
		/**
		* [EN]
		* Forward iterator over ArtMap entries in key order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ArtMap の要素をキー順に走査する前方イテレータ。
		*/
		struct Iterator
		{
			using iterator_category = std::forward_iterator_tag;
			using value_type = std::pair<const K, V>;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;

			Iterator() = default;

			/**
			* [EN]
			* Constructs an iterator positioned at the first (smallest-key) entry under root.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* root 以下の最初の（最小キーの）要素を指すイテレータを構築する。
			*/
			explicit Iterator(NodePtr root)
			{
				if (root)
				{
					stack_.push_back({ root, 0 });
					advance();
				}
			}

			/**
			* [EN]
			* Returns a reference to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの参照を返す。
			*/
			reference operator*()const
			{
				return current_->pair_;
			}

			/**
			* [EN]
			* Returns a pointer to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへのポインタを返す。
			*/
			pointer operator->()const
			{
				return &current_->pair_;
			}

			/**
			* [EN]
			* Advances to the next entry in key order (pre-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* キー順で次の要素へ進める（前置インクリメント）。
			*/
			Iterator& operator++()
			{
				stack_.pop_back();
				advance();
				return *this;
			}

			/**
			* [EN]
			* Advances to the next entry in key order (post-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* キー順で次の要素へ進める（後置インクリメント）。
			*/
			Iterator operator++(Int)
			{
				Iterator temporary = *this;
				++(*this);
				return temporary;
			}

			/**
			* [EN]
			* Returns whether both iterators point to the same entry (or are both end).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 両イテレータが同じ要素を指しているか（または両方とも終端か）を返す。
			*/
			Bool operator==(const Iterator& other)const noexcept
			{
				return current_ == other.current_;
			}

			/**
			* [EN]
			* Negation of operator==.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* operator== の否定。
			*/
			Bool operator!=(const Iterator& other)const noexcept
			{
				return !(*this == other);
			}

		private:
			/// [EN] Traversal stack driving advance().
			/// [JP] advance() を駆動する走査スタック。
			DynamicArray<Art::StackFrame> stack_;

			/// [EN] The entry this iterator currently points to (nullptr at end).
			/// [JP] このイテレータが現在指す要素（終端では nullptr）。
			Leaf* current_ = nullptr;

			/**
			* [EN]
			* Moves current_ to the next leaf via Art::descend.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* Art::descend 経由で current_ を次のリーフへ進める。
			*/
			void advance()
			{
				current_ = Art::descend<K, V>(stack_);
			}
		};

		/**
		* [EN]
		* Const forward iterator over ArtMap entries in key order (see Iterator).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ArtMap の要素をキー順に走査する const 前方イテレータ（Iterator を参照）。
		*/
		struct ConstIterator
		{
			using iterator_category = std::forward_iterator_tag;
			using value_type = std::pair<const K, V>;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;

			ConstIterator() = default;

			/**
			* [EN]
			* Constructs a const iterator positioned at the first (smallest-key) entry under root.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* root 以下の最初の（最小キーの）要素を指す const イテレータを構築する。
			*/
			explicit ConstIterator(NodePtr root)
			{
				if (root)
				{
					stack_.push_back({ root, 0 });
					advance();
				}
			}

			/**
			* [EN]
			* Converts a mutable Iterator to a ConstIterator at the same position.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 可変の Iterator を、同じ位置を指す ConstIterator に変換する。
			*/
			ConstIterator(const Iterator& it) : stack_(it.stack_), current_(it.current_)
			{
				/// No Code
			}

			/**
			* [EN]
			* Returns a const reference to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの const 参照を返す。
			*/
			reference operator*()const
			{
				return  current_->pair_;
			}

			/**
			* [EN]
			* Returns a const pointer to the current key/value pair.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 現在のキー/値ペアへの const ポインタを返す。
			*/
			pointer operator->()const
			{
				return &current_->pair_;
			}

			/**
			* [EN]
			* Advances to the next entry in key order (pre-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* キー順で次の要素へ進める（前置インクリメント）。
			*/
			ConstIterator& operator++()
			{
				stack_.pop_back();
				advance();
				return *this;
			}

			/**
			* [EN]
			* Advances to the next entry in key order (post-increment).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* キー順で次の要素へ進める（後置インクリメント）。
			*/
			ConstIterator operator++(Int)
			{
				ConstIterator temporary = *this;
				++(*this);
				return temporary;
			}

			/**
			* [EN]
			* Returns whether both iterators point to the same entry (or are both end).
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* 両イテレータが同じ要素を指しているか（または両方とも終端か）を返す。
			*/
			Bool operator==(const ConstIterator& other)const noexcept
			{
				return current_ == other.current_;
			}

			/**
			* [EN]
			* Negation of operator==.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* operator== の否定。
			*/
			Bool operator!=(const ConstIterator& other)const noexcept
			{
				return !(*this == other);
			}

		private:
			/// [EN] Traversal stack driving advance().
			/// [JP] advance() を駆動する走査スタック。
			DynamicArray<Art::StackFrame> stack_;

			/// [EN] The entry this iterator currently points to (nullptr at end).
			/// [JP] このイテレータが現在指す要素（終端では nullptr）。
			Leaf* current_ = nullptr;

			/**
			* [EN]
			* Moves current_ to the next leaf via Art::descend.
			*
			* ---------------------------------------------------------------------
			*
			* [JP]
			* Art::descend 経由で current_ を次のリーフへ進める。
			*/
			void advance()
			{
				current_ = Art::descend<K, V>(stack_);
			}
		};

	public:
		ArtMap() = default;

		/**
		* [EN]
		* Destroys every stored entry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 格納されている全要素を破棄する。
		*/
		~ArtMap()
		{
			Art::destroy<K, V>(root_);
		}

		ArtMap(const ArtMap&) = delete;
		ArtMap& operator=(const ArtMap&) = delete;

		/**
		* [EN]
		* Move-constructs by taking ownership of o's tree, leaving o empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* o の木の所有権を奪ってムーブ構築し、o は空になる。
		*/
		ArtMap(ArtMap&& o)noexcept : root_(o.root_), size_(o.size_)
		{
			o.root_ = nullptr;
			o.size_ = 0;
		}

		/**
		* [EN]
		* Constructs from an initializer list of key/value pairs, inserted
		* in list order (later duplicates of an existing key are ignored,
		* per insert's semantics).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* キー/値ペアの初期化子リストから構築する。リストの順に挿入する
		* （insert の仕様により、既存キーの後続の重複は無視される）。
		*/
		ArtMap(std::initializer_list<std::pair<const K, V>> init) :root_(nullptr), size_(0)
		{
			for (const auto& pair : init)
			{
				insert(pair.first, pair.second);
			}
		}

		/**
		* [EN]
		* Move-assigns by discarding this map's tree and taking ownership
		* of o's, leaving o empty.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このマップの木を破棄し、o の木の所有権を奪ってムーブ代入する。
		* o は空になる。
		*/
		ArtMap& operator=(ArtMap&& o)noexcept
		{
			if (this != &o)
			{
				Art::destroy<K, V>(root_);
				root_ = o.root_;
				size_ = o.size_;
				o.root_ = nullptr;
				o.size_ = 0;
			}
			return *this;
		}

		/**
		* [EN]
		* Clears this map, then inserts every pair from init in list order.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* このマップをクリアしたうえで、init の全ペアをリスト順に挿入する。
		*/
		ArtMap& operator=(std::initializer_list<std::pair<const K, V>> init)
		{
			clear();
			for (const auto& pair : init)
			{
				insert(pair.first, pair.second);
			}
			return *this;
		}

		/**
		* [EN]
		* Inserts key/value if key is not already present. Returns
		* true if inserted, false if key already existed (existing
		* value left unchanged).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key が未登録であれば key/value を挿入する。挿入した場合
		* true、key が既に存在した場合は false を返す（既存の値は
		* 変更しない）。
		*/
		Bool insert(const K& key, V value)
		{
			auto keyBite = Traits::to_bytes(key);
			return insert_implementation(root_, key, keyBite, std::move(value), 0);
		}

		/**
		* [EN]
		* Inserts key/value, overwriting the existing value if key is
		* already present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key/value を挿入する。key が既に存在する場合は既存の値を
		* 上書きする。
		*/
		void upsert(const K& key, V value)
		{
			auto keyBite = Traits::to_bytes(key);
			upsert_implementation(root_, key, keyBite, std::move(value), 0);
		}

		/**
		* [EN]
		* Invokes callback(const Leaf*) for every entry whose encoded key
		* starts with prefix's encoded bytes, without allocating a result array.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンコード済みキーが prefix のエンコード済みバイト列で始まる
		* 全要素について callback(const Leaf*) を呼び出す。結果配列の
		* 確保は行わない。
		*/
		template<class F>
		void prefix_search(const K& prefix, F&& callback)const
		{
			prefix_search_implementation(prefix, std::forward<F>(callback));
		}

		/**
		* [EN]
		* Returns pointers to every entry whose encoded key starts with
		* prefix's encoded bytes.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* エンコード済みキーが prefix のエンコード済みバイト列で始まる
		* 全要素へのポインタを返す。
		*/
		DynamicArray<const ValueType*> prefix_search(const K& prefix)const
		{
			DynamicArray<const ValueType*> result;

			prefix_search_implementation(prefix, [&](const Leaf* leaf) {result.push_back(&leaf->pair_);});

			return result;
		}

		/**
		* [EN]
		* Returns a copy of the value stored for key, or std::nullopt if not found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応する値のコピーを返す。見つからなければ std::nullopt。
		*/
		std::optional<V> find(const K& key)const
		{
			auto keyBite = Traits::to_bytes(key);
			Leaf* leaf = find_leaf(keyBite);
			if (!leaf)
			{
				return std::nullopt;
			}
			return leaf->pair_.second;
		}

		/**
		* [EN]
		* Returns a reference to the value stored for key. Throws
		* std::out_of_range if not found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key に対応する値への参照を返す。見つからなければ
		* std::out_of_range を送出する。
		*/
		V& at(const K& key)
		{
			auto keyBite = Traits::to_bytes(key);
			Leaf* leaf = find_leaf(keyBite);
			if (!leaf)
			{
				throw std::out_of_range("ArtMap::at: key not found");
			}
			return leaf->pair_.second;
		}

		/**
		* [EN]
		* Const overload of at.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* at の const オーバーロード。
		*/
		const V& at(const K& key)const
		{
			auto keyBite = Traits::to_bytes(key);
			const Leaf* leaf = find_leaf(keyBite);
			if (!leaf)
			{
				throw std::out_of_range("ArtMap::at: key not found");
			}
			return leaf->pair_.second;
		}

		/**
		* [EN]
		* Returns whether key is present.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key が存在するかを返す。
		*/
		Bool contains(const K& key)const
		{
			auto keyBite = Traits::to_bytes(key);
			return find_leaf(keyBite) != nullptr;
		}

		/**
		* [EN]
		* Removes key if present, rebalancing (shrinking) ancestor nodes
		* as needed. Returns whether an entry was removed.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* key が存在すれば削除し、必要に応じて祖先ノードを再調整
		* （縮小）する。要素が削除されたかを返す。
		*/
		Bool erase(const K& key)
		{
			auto keyBite = Traits::to_bytes(key);
			Bool removed = false;
			erase_implementation(root_, keyBite, 0, removed);
			return removed;
		}

		/**
		* [EN]
		* Returns the number of stored entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 格納されている要素数を返す。
		*/
		[[nodiscard]] Size size()const noexcept
		{
			return size_;
		}

		/**
		* [EN]
		* Returns whether the map has no entries.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* マップに要素が1つもないかを返す。
		*/
		[[nodiscard]] Bool empty()const noexcept
		{
			return size_ == 0;
		}

		/**
		* [EN]
		* Removes every entry.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 全要素を削除する。
		*/
		void clear()
		{
			Art::destroy<K, V>(root_);
			root_ = nullptr;
			size_ = 0;
		}

		/**
		* [EN]
		* Returns an iterator to the first entry (smallest key).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初の要素（最小キー）を指すイテレータを返す。
		*/
		Iterator begin()
		{
			return Iterator(root_);
		}

		/**
		* [EN]
		* Returns the past-the-end iterator.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 終端の次を指すイテレータを返す。
		*/
		Iterator end()
		{
			return Iterator();
		}

		/**
		* [EN]
		* Const overload of begin().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* begin() の const オーバーロード。
		*/
		ConstIterator begin()const
		{
			return ConstIterator(root_);
		}

		/**
		* [EN]
		* Const overload of end().
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* end() の const オーバーロード。
		*/
		ConstIterator end()const
		{
			return ConstIterator();
		}

		/**
		* [EN]
		* Returns an explicitly const iterator to the first entry (smallest key).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 最初の要素（最小キー）を指す、明示的な const イテレータを返す。
		*/
		ConstIterator cbegin()const
		{
			return ConstIterator(root_);
		}

		/**
		* [EN]
		* Returns the explicitly const past-the-end iterator.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 終端の次を指す、明示的な const イテレータを返す。
		*/
		ConstIterator cend()const
		{
			return ConstIterator();
		}

	private:
		/// [EN] Root of the ART, or nullptr if the map is empty.
		/// [JP] ART のルート。マップが空なら nullptr。
		NodePtr root_ = nullptr;

		/// [EN] Number of stored entries.
		/// [JP] 格納されている要素数。
		Size size_ = 0;

		/**
		* [EN]
		* Walks the tree from the root, following the byte at each depth,
		* to locate the leaf whose encoded key equals key. Returns
		* nullptr if not found.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ルートから木を辿り、各深さのバイトに従って進み、エンコード済み
		* キーが key と等しいリーフを探す。見つからなければ nullptr。
		*/
		Leaf* find_leaf(std::span<const Uint8> key)const
		{
			NodePtr node = root_;
			Uint32 depth = 0;

			while (node)
			{
				if (node->type_ == Art::NodeType::Leaf)
				{
					auto* leaf = Art::as_leaf<K, V>(node);
					return leaf->template matches<Traits>(key) ? leaf : nullptr;
				}
 
				Uint32 prefix = node->check_prefix(key, depth);
				if (prefix < node->prefixLength_)
				{
					return nullptr;
				}
				depth += node->prefixLength_;
				if (depth >= key.size())
				{
					return nullptr;
				}

				NodePtr* child = Art::find(node, key[depth]);
				if (!child)
				{
					return nullptr;
				}
				node = *child;
				++depth;
			}
			return nullptr;
		}

		/**
		* [EN]
		* Walks the tree from the root to find the subtree node whose
		* encoded key path exactly covers prefix (i.e. the root of the
		* subtree containing every entry whose key starts with prefix).
		* Returns nullptr if no entry has that prefix.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* ルートから木を辿り、エンコード済みキー経路がちょうど prefix を
		* 覆うサブツリーのノード（すなわち、キーが prefix で始まる
		* 全要素を含むサブツリーのルート）を探す。その接頭辞を持つ要素が
		* なければ nullptr。
		*/
		NodePtr find_prefix_node(std::span<const Uint8> prefix)const
		{
			NodePtr node = root_;
			Uint32 depth = 0;

			while (node)
			{
				if (node->type_ == Art::NodeType::Leaf)
				{
					auto* leaf = Art::as_leaf<K, V>(node);
					auto bytes = Traits::to_bytes(leaf->pair_.first);

					if (bytes.size() < prefix.size())
					{
						return nullptr;
					}

					if (std::equal(prefix.begin(), prefix.end(), bytes.begin()))
					{
						return node;
					}

					return nullptr;
				}

				Uint32 matched = node->check_prefix(prefix, depth);

				if (depth + matched == prefix.size())
				{
					return node;
				}

				if (matched < node->prefixLength_)
				{
					return nullptr;
				}

				depth += node->prefixLength_;

				NodePtr* child = Art::find(node, prefix[depth]);
				if (!child)
				{
					return nullptr;
				}

				node = *child;
				++depth;

				if (depth >= prefix.size())
				{
					return node;
				}
			}

			return nullptr;
		}

		/**
		* [EN]
		* Recursive worker for insert: descends/splits nodes as needed to
		* place a new leaf for originalKey/value at key[depth..],
		* stopping without modification if an equal key already exists.
		* Returns whether a new entry was inserted.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* insert の再帰ワーカー。originalKey/value の新しいリーフを
		* key[depth..] の位置に配置するため、必要に応じてノードを降下・
		* 分割する。等しいキーが既に存在する場合は変更せずに終了する。
		* 新しい要素が挿入されたかを返す。
		*/
		Bool insert_implementation(NodePtr& node, const K& originalKey, std::span<const Uint8> key, V value, Uint32 depth)
		{
			if (!node) 
			{
				node = new Leaf(originalKey, std::move(value));
				++size_;
				return true;
			}

			if (node->type_ == Art::NodeType::Leaf)
			{
				auto* existing = Art::as_leaf<K, V>(node);
				if (existing->template matches<Traits>(key))
				{
					return false;
				}

				auto* newNode = new Art::Node4();
				Uint32 common = 0;
				auto existingBytes = Traits::to_bytes(existing->pair_.first);
				while (depth + common < existingBytes.size() && depth + common < key.size() && existingBytes[depth + common] == key[depth + common])
				{
					++common;
				}

				newNode->prefixLength_ = static_cast<Uint8>(Min<Uint32>(common, Art::MAX_PREFIX));
				std::memcpy(newNode->prefix_, existingBytes.data() + depth, newNode->prefixLength_);

				Uint32 splitDepth = depth + common;
				Uint8 oldByte = (splitDepth < existingBytes.size()) ? existingBytes[splitDepth] : 0;
				Uint8 newByte = (splitDepth < key.size()) ? key[splitDepth] : 0;

				newNode->add(oldByte, existing);
				auto* newLeaf = new Leaf(originalKey, std::move(value));
				newNode->add(newByte, newLeaf);

				node = newNode;
				++size_;
				return true;
			}

			Uint32 prefix = node->check_prefix(key, depth);
			if (prefix < node->prefixLength_) 
			{
				auto* newNode = new Art::Node4();
				newNode->prefixLength_ = static_cast<Uint8>(prefix);
				std::memcpy(newNode->prefix_, node->prefix_, prefix);

				auto* anyLeaf = Art::minimum<K, V>(node);
				auto anyBytes = Traits::to_bytes(anyLeaf->pair_.first);
				Uint8 oldByte = node->prefix_[prefix];
				newNode->add(oldByte, node);
				node->prefixLength_ -= (prefix + 1);
				std::memmove(node->prefix_, anyBytes.data() + depth + prefix + 1, Min<Uint32>(node->prefixLength_, Art::MAX_PREFIX));

				Uint8 newByte = (depth + prefix < key.size()) ? key[depth + prefix] : 0;
				auto* newLeaf = new Leaf(originalKey, std::move(value));
				newNode->add(newByte, newLeaf);

				node = newNode;
				++size_;
				return true;
			}

			depth += node->prefixLength_;
			if (depth >= key.size())
			{
				return false;
			}

			NodePtr* child = Art::find(node, key[depth]);
			if (child) 
			{
				return insert_implementation(*child, originalKey, key, std::move(value), depth + 1);
			}
			auto* newLeaf = new Leaf(originalKey, std::move(value));
			Art::add(node, key[depth], newLeaf);
			++size_;
			return true;
		}

		/**
		* [EN]
		* Recursive worker for upsert: like insert_implementation, but
		* overwrites the existing leaf's value in place when key already
		* exists instead of leaving it unchanged.
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* upsert の再帰ワーカー。insert_implementation と似ているが、
		* key が既に存在する場合は変更せずに終えるのではなく、既存リーフの
		* 値をその場で上書きする。
		*/
		void upsert_implementation(NodePtr& node, const K& originalKey, std::span<const Uint8> key, V value, Uint32 depth)
		{
			if (!node) 
			{
				node = new Leaf(originalKey, std::move(value));
				++size_;
				return;
			}

			if (node->type_ == Art::NodeType::Leaf)
			{
				auto* existing = Art::as_leaf<K, V>(node);
				if (existing->template matches<Traits>(key))
				{
					existing->pair_.second = std::move(value);
					return;
				}
				Bool added = insert_implementation(node, originalKey, key, std::move(value), depth);
				(void)added;
				return;
			}

			Uint32 prefix = node->check_prefix(key, depth);
			if (prefix < node->prefixLength_) 
			{
				insert_implementation(node, originalKey, key, std::move(value), depth);
				return;
			}
			depth += node->prefixLength_;
			if (depth >= key.size())
			{
				return;
			}

			NodePtr* child = Art::find(node, key[depth]);
			if (child) 
			{
				upsert_implementation(*child, originalKey, key, std::move(value), depth + 1);
				return;
			}

			auto* newLeaf = new Leaf(originalKey, std::move(value));
			Art::add(node, key[depth], newLeaf);
			++size_;
		}

		/**
		* [EN]
		* Recursive worker for erase: descends to the leaf matching key,
		* deletes it, sets removed to true if found, and on the way back
		* up path-compresses any ancestor left with a single child (merging
		* its prefix with that child's).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* erase の再帰ワーカー。key に一致するリーフまで降下して削除し、
		* 見つかった場合は removed を true にする。呼び出し元へ戻る際、
		* 子が1つだけになった祖先ノードがあればパス圧縮する（その子の
		* 接頭辞と結合する）。
		*/
		void erase_implementation(NodePtr& node, std::span<const Uint8> key, Uint32 depth, Bool& removed)
		{
			if (!node)
			{
				return;
			}

			if (node->type_ == Art::NodeType::Leaf)
			{
				auto* leaf = Art::as_leaf<K, V>(node);
				if (leaf->template matches<Traits>(key))
				{
					delete leaf;
					node = nullptr;
					--size_;
					removed = true;
				}
				return;
			}

			Uint32 prefix = node->check_prefix(key, depth);
			if (prefix < node->prefixLength_)
			{
				return;
			}
			depth += node->prefixLength_;
			if (depth >= key.size())
			{
				return;
			}

			NodePtr* child = Art::find(node, key[depth]);
			if (!child)
			{
				return;
			}

			if ((*child)->type_ == Art::NodeType::Leaf) 
			{
				auto* leaf = Art::as_leaf<K, V>(*child);
				if (leaf->template matches<Traits>(key))
				{
					delete leaf;
					*child = nullptr;
					Art::remove(node, key[depth]);
					--size_;
					removed = true;
				}
			}
			else
			{
				erase_implementation(*child, key, depth + 1, removed);
			}

			if (removed && node)
			{
				if (node->numberChildren_ == 1)
				{
					NodePtr onlyChild = nullptr;
					Uint8 onlyChildKeyByte = 0;

					switch (node->type_)
					{
					case Art::NodeType::Node4:
					{
						auto* node4 = static_cast<Art::Node4*>(node);
						for (Uint32 index = 0; index < 4; ++index)
						{
							if (node4->children_[index] != nullptr)
							{
								onlyChild = node4->children_[index];
								onlyChildKeyByte = node4->keys_[index];
								break;
							}
						}
						break;
					}
					case Art::NodeType::Node16:
					{
						auto* node16 = static_cast<Art::Node16*>(node);
						for (Uint32 index = 0; index < 16; ++index)
						{
							if (node16->children_[index] != nullptr)
							{
								onlyChild = node16->children_[index];
								onlyChildKeyByte = node16->keys_[index];
								break;
							}
						}
						break;
					}
					case Art::NodeType::Node48:
					{
						auto* node48 = static_cast<Art::Node48*>(node);
						for (Uint32 index = 0; index < 256; ++index)
						{
							if (node48->index_[index] != 0xFF)
							{
								onlyChild = node48->children_[node48->index_[index]];
								onlyChildKeyByte = static_cast<Uint8>(index);
								break;
							}
						}
						break;
					}
					case Art::NodeType::Node256:
					{
						auto* node256 = static_cast<Art::Node256*>(node);
						for (Uint32 index = 0; index < 256; ++index)
						{
							if (node256->children_[index] != nullptr)
							{
								onlyChild = node256->children_[index];
								onlyChildKeyByte = static_cast<Uint8>(index);
								break;
							}
						}
						break;
					}
					default:
						break;
					}

					if (onlyChild != nullptr)
					{
						if (onlyChild->type_ != Art::NodeType::Leaf)
						{
							Uint32 newPrefixLength = node->prefixLength_ + 1 + onlyChild->prefixLength_;

							if (newPrefixLength <= Art::MAX_PREFIX)
							{
								Uint8 temporaryPrefix[Art::MAX_PREFIX] = { 0 };
								Uint32 cacheIndex = 0;

								for (Uint32 index = 0; index < node->prefixLength_; ++index)
								{
									temporaryPrefix[cacheIndex++] = node->prefix_[index];
								}
								temporaryPrefix[cacheIndex++] = onlyChildKeyByte;
								for (Uint32 index = 0; index < onlyChild->prefixLength_; ++index)
								{
									temporaryPrefix[cacheIndex++] = onlyChild->prefix_[index];
								}

								onlyChild->prefixLength_ = newPrefixLength;
								for (Uint32 index = 0; index < newPrefixLength; ++index)
								{
									onlyChild->prefix_[index] = temporaryPrefix[index];
								}

								NodePtr oldNode = node;
								node = onlyChild;
								delete oldNode;
							}
						}
						else
						{
							NodePtr oldNode = node;
							node = onlyChild;
							delete oldNode;
						}
					}
				}
			}
		}

		/**
		* [EN]
		* Shared worker for both prefix_search overloads: locates the
		* subtree covering prefix via find_prefix_node, then visits
		* every leaf under it in key order, invoking callback(const Leaf*).
		*
		* ---------------------------------------------------------------------
		*
		* [JP]
		* 両方の prefix_search オーバーロードで共有されるワーカー。
		* find_prefix_node で prefix を覆うサブツリーを探し、その下の
		* 全リーフをキー順に訪問して callback(const Leaf*) を呼び出す。
		*/
		template<class F>
		void prefix_search_implementation(const K& prefix, F&& callback)const
		{
			auto prefixBytes = Traits::to_bytes(prefix);

			NodePtr node = find_prefix_node(prefixBytes);

			if (!node)
			{
				return;
			}

			DynamicArray<Art::StackFrame> stack;
			stack.push_back({ node, 0 });

			while (auto* leaf = Art::descend<K, V>(stack))
			{
				callback(leaf);
				stack.pop_back();
			}
		}
	};
}
