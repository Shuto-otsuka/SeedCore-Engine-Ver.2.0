#pragma once
#include <FoundationEngine/Prelude.h>

namespace SeedCore
{
	struct EditorContext;

	enum class IconType : Uint
	{
		FolderInItem,
		FolderNoItem,

		Model,
		Effect,
		Audio,
		Font,
		Sky,
		Animation,
		MeshCollision,
		Material,
		Skeleton,
		Movie,

		Text,
		Cpp,
		Header,
		Hlsl,
		Search,

		Play,
		Pause,
		Stop,

		Actor,
		ActorChild,
		Prefab,
		PrefabChild,
		Scene,

		Lock,
		LockFree,

		Guizmo,
		NonSelected,
		Translate,
		Rotate,
		Scale,
		Camera,
		ViewMode,

		LogError,
		LogWarning,
		LogNotice,

		NonCameraWarning,

		ActorActive,
		ActorNonActive,

		/// [EN] Per-component icons shown next to each component's header in
		///      the Inspector (Unity-style). CustomComponent is the fallback
		///      for any component not explicitly mapped — user scripts and
		///      any built-in component not given its own icon.
		/// [JP] Inspector の各コンポーネントヘッダーの隣に表示する、
		///      コンポーネントごとのアイコン（Unity 風）。CustomComponent は
		///      明示的に対応付けられていないコンポーネント（ユーザー
		///      スクリプトや、専用アイコンを持たない組み込みコンポーネント）
		///      のフォールバック。
		ComponentTransform,
		ComponentCamera,
		ComponentFreeCameraController,
		ComponentOrbitCameraController,
		ComponentPointLight,
		ComponentDirectionalLight,
		ComponentSpotLight,
		ComponentRectangleLight,
		ComponentSkyLight,
		ComponentBoxCollider,
		ComponentSphereCollider,
		ComponentCapsuleCollider,
		ComponentCylinderCollider,
		ComponentRectCollider,
		ComponentCircleCollider,
		ComponentMeshCollider,
		ComponentRigidbody,
		ComponentSoftbody,
		ComponentCharacterController,
		ComponentHingeJoint,
		ComponentFixedJoint,
		ComponentSpringJoint,
		ComponentSliderJoint,
		ComponentAudioSource,
		ComponentAudioListener,
		ComponentImage,
		ComponentText,
		ComponentMovie,
		ComponentMesh,
		ComponentMaterial,
		ComponentSkeleton,
		ComponentAnimator,
		ComponentPositionConstraint,
		ComponentRotationConstraint,
		ComponentLookAtConstraint,
		ComponentParentConstraint,
		ComponentAttachmentConstraint,
		ComponentIKConstraint,
		ComponentWeather,
		ComponentEffect,
		ComponentPostProcess,
		ComponentSpawner,
		ComponentLifetime,
		ComponentCustom,

		Count
	};

	class ImGuiTexture
	{
	public:
		ImGuiTexture(EditorContext& context);
		~ImGuiTexture() = default;

		[[nodiscard]] ImTextureID Icon(IconType type)const;

		/// [EN] Maps a component's registered name to its Inspector/Add
		///      Component header icon (Unity-style). Falls back to
		///      IconType::ComponentCustom for anything not explicitly
		///      listed — covers UserProject scripts and any built-in
		///      component without a dedicated icon. Static (no instance
		///      state needed) so both InspectorPanel and AddComponentPanel
		///      can share one lookup table instead of each keeping their own.
		/// [JP] コンポーネントの登録名を Inspector/Add Component ヘッダー用
		///      アイコン（Unity 風）へ対応付ける。明示的に列挙されていない
		///      ものは IconType::ComponentCustom にフォールバックする —
		///      UserProject のスクリプトや、専用アイコンを持たない組み込み
		///      コンポーネントをカバーする。static（インスタンス状態不要）
		///      にすることで、InspectorPanel と AddComponentPanel が
		///      それぞれ別の対応表を持たず、1つを共有できる。
		[[nodiscard]] static IconType ComponentIconType(const String& componentName);

	private:
		ImTextureID icons_[static_cast<Uint>(IconType::Count)] = {};

		DynamicArray<Microsoft::WRL::ComPtr<ID3D12Resource>> resources_;
	};
}
