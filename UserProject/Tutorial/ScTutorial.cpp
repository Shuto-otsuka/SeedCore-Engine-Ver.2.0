#include "UserProject/Tutorial/ScTutorial.h"
//#include <FoundationEngine/ECS/World.h>
//#include <FoundationEngine/ECS/Component/Position.h>
//#include <FoundationEngine/ECS/Component/Rotation.h>
//#include <FoundationEngine/ECS/Component/Scale.h>
//#include <FoundationEngine/ECS/Component/Velocity.h>

void ScTutorial::OnAwake()
{
	//demo1_ = 1.0f;
}

void ScTutorial::OnStart()
{
	//auto& world = GetWorld();
	//SeedCore::Entity entity = GetActor().GetEntity();
	//SeedCore::Position* position = world.GetComponent<SeedCore::Position>(entity);
	//SeedCore::Rotation* rotation = world.GetComponent<SeedCore::Rotation>(entity);
	//SeedCore::Scale* scale = world.GetComponent<SeedCore::Scale>(entity);
	//SeedCore::Velocity* velocity = world.GetComponent<SeedCore::Velocity>(entity);
	//
	//position->x = 0;
	//position->y = 1;
	//position->z = 0;
}	

void ScTutorial::OnTick(float elapsedTime)
{

}

void ScTutorial::OnLateTick(float elapsedTime)
{

}

void ScTutorial::OnFixedTick(float elapsedTime)
{

}

void ScTutorial::OnDestroy()
{
	//demo1_ = 0.0f;
}

void ScTutorial::OnInspectorGUI()
{
	//ImGui::Text("デモ");
}

void ScTutorial::OnCollisionEnter(SeedCore::Entity entity)
{

}

void ScTutorial::OnCollisionStay(SeedCore::Entity entity)
{

}

void ScTutorial::OnCollisionExit(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerEnter(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerStay(SeedCore::Entity entity)
{

}

void ScTutorial::OnTriggerExit(SeedCore::Entity entity)
{

}