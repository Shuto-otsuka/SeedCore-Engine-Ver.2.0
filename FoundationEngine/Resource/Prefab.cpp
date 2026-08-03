#include <FoundationEngine/Resource/Prefab.h>
#include <FoundationEngine/ECS/Actor.h>
#include <FoundationEngine/ECS/World.h>
#include <FoundationEngine/ECS/Component/Name.h>
#include <FoundationEngine/Log/Error.h>

namespace SeedCore
{
	/**
	* [EN]
	* Records root and every descendant into nodes_, replacing any
	* previously captured data.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* root とその全子孫を nodes_ へ記録し、以前に取得していたデータを
	* 置き換える。
	*/
	void Prefab::Capture(Actor* root)
	{
		nodes_.clear();
		CaptureActorNode(root, -1, nodes_);
	}

	/**
	* [EN]
	* Recreates the captured subtree as new actors in world, parented
	* under parent (if given). Returns the root of the newly created
	* subtree, or nullptr if nothing was instantiated. sourceAssetID is
	* stamped onto the new root so later "apply to prefab" edits know
	* which .prefab asset to overwrite.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* 取得済みのサブツリーを、world 内に新しい actor 群として再生成する。
	* parent が指定されていればその下へ配置する。新しく生成された
	* サブツリーのルートを返す。何もインスタンス化されなければ nullptr
	* を返す。sourceAssetID は新しいルートへ刻印され、後の
	* 「プレハブに適用」編集がどの .prefab アセットを上書きすべきかを
	* 判断できるようにする。
	*/
	Actor* Prefab::Instantiate(World& world, ResourceCache& cache, Actor* parent, Uint32 sourceAssetID)const
	{
		DynamicArray<Actor*> instantiated;
		instantiated.reserve(nodes_.size());

		/// [EN] Nodes are stored in capture order, so each node's parent (identified by an earlier index) has always already been instantiated by the time we reach it.
		/// [JP] ノードは取得順に格納されているため、各ノードの親（それより前のインデックスで識別される）は、そこに到達する時点で既にインスタンス化済みである。
		for (Size index = 0; index < nodes_.size(); ++index)
		{
			const PrefabNode& node = nodes_[index];

			Actor* parentActor = nullptr;
			if (node.parentIndex_ >= 0 && static_cast<Size>(node.parentIndex_) < instantiated.size())
			{
				parentActor = instantiated[node.parentIndex_];
			}
			else if (index == 0)
			{
				/// [EN] The root node has no in-subtree parent: attach it to the caller-supplied parent instead.
				/// [JP] ルートノードはサブツリー内に親を持たない: 代わりに呼び出し側が指定した parent へアタッチする。
				parentActor = parent;
			}

			Actor* actor = InstantiateActorNode(world, cache, node, parentActor, true);

			if (actor && index == 0)
			{
				actor->SetSourcePrefabAssetID(sourceAssetID);
			}

			instantiated.push_back(actor);
		}

		return instantiated.empty() ? nullptr : instantiated[0];
	}

	/**
	* [EN]
	* Writes this prefab's captured data to path as JSON. Returns
	* whether the file was written successfully.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* この prefab の取得済みデータを、JSON として path へ書き込む。
	* ファイルが正常に書き込まれたかどうかを返す。
	*/
	Bool Prefab::Write(const std::filesystem::path& path)
	{
		std::ofstream ofs(path);
		if (!ofs)
		{
			return false;
		}

		cereal::JSONOutputArchive archive(ofs);
		archive(*this);

		return true;
	}

	/**
	* [EN]
	* Reads captured data from path (JSON), replacing this prefab's
	* current data. Returns whether reading succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* path（JSON）から取得済みデータを読み込み、この prefab の現在の
	* データを置き換える。読み込みに成功したかどうかを返す。
	*/
	Bool Prefab::Read(const std::filesystem::path& path)
	{
		std::ifstream ifs(path);
		if (!ifs)
		{
			return false;
		}

		try
		{
			cereal::JSONInputArchive archive(ifs);
			archive(*this);
		}
		catch (const cereal::Exception& exception)
		{
			SC_LOG_ERROR("Prefabの読み込みに失敗しました ({}): {}", path.string(), exception.what());
			return false;
		}

		return true;
	}

	/**
	* [EN]
	* Captures root into a temporary Prefab and writes it to path,
	* creating parent directories as needed. Returns whether saving succeeded.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* root を一時的な Prefab へ取得し、path へ書き込む（必要なら親
	* ディレクトリを作成する）。保存に成功したかどうかを返す。
	*/
	Bool Prefab::Save(Actor* root, const std::filesystem::path& path)
	{
		Prefab prefab;
		prefab.Capture(root);

		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path());
		}

		return prefab.Write(path);
	}

	/**
	* [EN]
	* Captures root and saves it into directory under a name derived
	* from root's Name component (or "Actor" if unnamed), appending a
	* numeric suffix to avoid overwriting an existing file. Returns the
	* path actually written to, or an empty path on failure.
	*
	* ---------------------------------------------------------------------
	*
	* [JP]
	* root を取得し、その Name コンポーネントから導出された名前（名前が
	* 無ければ "Actor"）で directory 内へ保存する。既存ファイルの上書きを
	* 避けるため、数値の接尾辞を付加する。実際に書き込まれたパスを返す。
	* 失敗時は空のパスを返す。
	*/
	std::filesystem::path Prefab::SaveToDirectory(Actor* root, const std::filesystem::path& directory)
	{
		Prefab prefab;
		prefab.Capture(root);

		std::filesystem::create_directories(directory);

		const Name* name = root->GetComponent<Name>();
		std::string baseName = (name && !name->name_.str().empty()) ? name->name_.str() : "Actor";

		/// [EN] Probe for a free filename, appending "(N)" until one doesn't already exist.
		/// [JP] 空いているファイル名を探索し、既に存在しなくなるまで "(N)" を付加する。
		std::filesystem::path prefabPath = directory / (baseName + ".prefab");
		Int suffix = 1;
		while (std::filesystem::exists(prefabPath))
		{
			prefabPath = directory / (baseName + "(" + std::to_string(suffix++) + ").prefab");
		}

		if (!prefab.Write(prefabPath))
		{
			return {};
		}

		return prefabPath;
	}
}
