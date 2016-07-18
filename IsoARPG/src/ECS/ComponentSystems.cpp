#include "ECS/ComponentSystems.h"
#include "ECS/LabelSystem.h"
#include "ECS/AttributeSystem.h"
#include "ECS/PlayerControllerSystem.h"
#include "ECS/Transform3DSystem.h"
#include "ECS/CollisionSystem.h"
#include "ECS/Animation2DSystem.h"
#include "ECS/InventorySystem.h"
#include "ECS/Renderer2DSystem.h"
#include "ECS/AIControllerSystem.h"
#include "ECS/EffectSystem.h"
#include "Utils/Errors.h"
#include "Math/Random.h"
#include "Graphics/SpriteSheet.h"
#include "IO/ResourceManager.h"
#include "Defines.h"


#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <algorithm>


void printDebug(char* message);

namespace ECS { namespace Systems {

	///////////////////////////
	// Entity Manager System //
	///////////////////////////

	namespace EntitySystem {

		eid32 FindNextAvailableEntity(EntityManager* Manager);

		// Creates new EntityManager and returns it
		struct EntityManager* NewEntityManager(int Width, int Height, Enjon::Graphics::Camera2D* Camera, Level* Lvl)
		{
			struct EntityManager* Manager = new EntityManager;

			if (Manager == nullptr) Enjon::Utils::FatalError("COMPOPNENT_SYSTEMS::NEW_ENTITY_MANAGER::Manager is null"); 

			// Initialize NextAvailableID to 0
			Manager->NextAvailableID = 0;

			// Initialize Length to 0
			Manager->Length = 0;

			// Initialize LastUsedID to 0
			Manager->MaxAvailableID = 0;

			// Initialize LastUsedID to 0
			Manager->MinID = 0;

			// Initialize the component systems
			Manager->TransformSystem			= Transform::NewTransform3DSystem(Manager);
			Manager->PlayerControllerSystem 	= PlayerController::NewPlayerControllerSystem(Manager);
			Manager->AIControllerSystem			= AIController::NewAIControllerSystem(Manager);
			Manager->LabelSystem				= Label::NewLabelSystem(Manager);
			Manager->Animation2DSystem			= Animation2D::NewAnimation2DSystem(Manager);
			Manager->AttributeSystem 			= Attributes::NewAttributeSystem(Manager);
			Manager->Renderer2DSystem 			= Renderer2D::NewRenderer2DSystem(Manager);
			Manager->InventorySystem 			= Inventory::NewInventorySystem(Manager);
			Manager->CollisionSystem 			= Collision::NewCollisionSystem(Manager);
			Manager->EffectSystem				= Effect::NewEffectSystem(Manager);
			Manager->ParticleEngine 			= EG::Particle2D::NewParticleEngine();

			Manager->Width = Width;
			Manager->Height = Height;

			// Initialize spatial hash
			Manager->Grid = new SpatialHash::Grid;
			SpatialHash::Init(Manager->Grid, Manager->Width, Manager->Height);

			// Set up camera
			Manager->Camera = Camera;

			// Set level
			Manager->Lvl = Lvl;

			return Manager;
		} 

		// NOTE(John): Creating and Removing entities is buggy right now in the way that it manages ids
		// TODO(John): This is busted, so fix it to be similar to how particle batches create particles
		// Creates blank entity and returns eid

		/*
		eid32 CreateEntity(struct EntityManager* Manager, bitmask32 Components)
		{
			eid32 Entity;

			// Generate a unique id for our entity if possible
			if (Manager->Length != MAX_ENTITIES) 
			{
				// Set id of entity to next available one
				Entity = Manager->NextAvailableID; 

				// Push back into map
				Manager->Entities.push_back(Entity);

				// Increment both NextAvailableID and Length if they're equal
				if (Manager->NextAvailableID == Manager->MaxAvailableID) 
				{
					Manager->NextAvailableID++;
					Manager->MaxAvailableID++;
				}
				else
				{
					// Otherwise set next to length, which is the greatest unused id
					Manager->NextAvailableID = Manager->MaxAvailableID;
				} 

				// Increment Length
				Manager->Length++;
				
				// Set bitfield up
				Manager->Masks[Entity] = Components;
				
				return Entity;
			}
			
			// Otherwise return MAX_ENTITIES as an error
			return MAX_ENTITIES;
		}
		*/

		eid32 CreateEntity(struct EntityManager* Manager, bitmask32 Components)
		{
			eid32 Entity = FindNextAvailableEntity(Manager);

			// Find next available ID and assign to entity
			if (Entity < MAX_ENTITIES)
			{
				// Increment Length
				Manager->Length++;

				Manager->Entities.push_back(Entity);

				// Set bitfield up
				Manager->Masks[Entity] = Components;

				// Set max id
				if (Manager->MaxAvailableID < Manager->NextAvailableID) Manager->MaxAvailableID = Manager->NextAvailableID;

				return Entity;	
			}
			
			// Otherwise return MAX_ENTITIES as an error
			return MAX_ENTITIES;
		}

		eid32 FindNextAvailableEntity(EntityManager* Manager)
		{
			// Get next available id
			auto NAID = Manager->NextAvailableID;
			
			for (auto i = NAID; i < MAX_ENTITIES - 1; ++i)
			{
				if (Manager->Masks[i] == COMPONENT_NONE)
				{
					Manager->NextAvailableID = i+1;
					return i;
				}
			}

			// Loop from beginning to NAID - 1
			for (auto i = 0; i < NAID - 2; ++i)
			{
				if (Manager->Masks[i] == COMPONENT_NONE)
				{
					Manager->NextAvailableID = i+1;
					return i;
				}
			}

			// Not sure about this...
			Manager->NextAvailableID = MAX_ENTITIES;
			return MAX_ENTITIES;
		}

		// Removes selected entity from manager by setting bitfield to COMPONENT_NONE
		void RemoveEntity(struct EntityManager* Manager, eid32 Entity)
		{
			// Need to reset/remove all of its components in here
			// TODO(John): Keep track of all its components (maps) and then remove all of those
			if (Manager->AttributeSystem->Masks[Entity] & Masks::Type::WEAPON) 
			{
				// Manager->AttributeSystem->WeaponProfiles.erase(Entity);
			}

			// Get handle to entity mask
			auto Mask = Manager->Masks[Entity];

			// Reset Transform component
			if (Mask & COMPONENT_TRANSFORM3D)
			{
				Transform::Reset(Manager, Entity);
			}

			// Reset AIController component
			if (Mask & COMPONENT_AICONTROLLER)
			{
				AIController::Reset(Manager, Entity);
			}

			// Reset Attribute System
			Attributes::Reset(Manager, Entity);

			// Set component mask to COMPONENT_NONE to remove
			Manager->Masks[Entity] = COMPONENT_NONE;

			// Not sure about that...
			if (Entity < Manager->MinID) Manager->MinID = Entity;

			// Decrement length
			if (Manager->Length > 0) Manager->Length--; 

			// Pop entity
			if (Manager->Entities.size()) Manager->Entities.pop_back();

			// Set next available entity id to entity
			Manager->NextAvailableID = Entity; 
		}

		// Verifies whether or not entity is alive
		bool IsAlive(struct EntityManager* Manager, eid32 Entity)
		{
			// If alive, then its component mask will not equal COMPONENT_NONE
			return Manager->Masks[Entity] != COMPONENT_NONE;
		}

		// Turns off component
		void RemoveComponents(struct EntityManager* Manager, eid32 Entity, bitmask32 Components)
		{
			Manager->Masks[Entity] &= ~(Components);
		}

	} // namespace EntitySystem
}}

void printDebug(char* message)
{
	static int counter = 0.0f;
	counter += 0.1f;
	if (counter > 2.0f)
	{
		printf(message);
		counter = 0.0f;
	}
}



