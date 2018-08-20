
// Copyright 2016-2017 John Jackson. All Rights Reserved.
// File: ObjectArchiver.h

#include "Serialize/ObjectArchiver.h"
#include "Serialize/EntityArchiver.h"
#include "Serialize/BaseTypeSerializeMethods.h"
#include "Graphics/Color.h"													// Don't like this here, but I'll leave it for now
#include "Serialize/UUID.h"
#include "System/Types.h"
#include "Defines.h"
#include "Asset/AssetManager.h"
#include "Math/Transform.h"
#include "SubsystemCatalog.h"
#include "Engine.h"

namespace Enjon
{ 
	//=====================================================================

	ObjectArchiver::ObjectArchiver( )
	{ 
	}

	//=====================================================================

	ObjectArchiver::~ObjectArchiver( )
	{ 
	}

	//===================================================================== 
 
	Result ObjectArchiver::Serialize( const Object* object )
	{
		// Make call to static function and return result
		return Serialize( object, &mBuffer );
	}

	//=====================================================================

	Result ObjectArchiver::Serialize( const Object* object, ByteBuffer* buffer )
	{ 
		// Get meta class from object
		const MetaClass* cls = object->Class( ); 

		// TODO(): Get versioning structure from meta class this for object...  
		/*
			const MetaClassVersionArchive* archive = cls->GetVersionArchive();
			buffer->Write( archive->GetClassName() );		
			buffer->Write( archive->GetVersionNumber() );
		*/

		// Write out class header information using meta class
		// Class name
		if ( cls )
		{ 
			buffer->Write< String >( cls->GetName( ) );
			// Make shift version number
			buffer->Write( 0 );		

			// Serialize all object specific data ( classes can override at this point how they want to serialize data )
			Result res = object->SerializeData( buffer );

			// Continue with default serialization if the object doesn't handle its own
			if ( res == Result::INCOMPLETE )
			{
				res = SerializeObjectDataDefault( object, cls, buffer );
			}

			// Final result of serialization
			return res;
		}

		// Shouldn't reach here
		return Result::FAILURE; 
	}

	//=====================================================================
 
	Result ObjectArchiver::SerializeObjectDataDefault( const Object* object, const MetaClass* cls )
	{ 
		// Call to static method and return result
		return ObjectArchiver::SerializeObjectDataDefault( object, cls, &mBuffer ); 
	}

	//=====================================================================

	Result ObjectArchiver::SerializeObjectDataDefault( const Object* object, const MetaClass* cls, ByteBuffer* buffer )
	{ 
		// Write out total count of serializable properties
		buffer->Write< usize >( cls->GetSerializablePropertyCount( ) ); 

		// Serialize all object properties
		for ( usize i = 0; i < cls->GetPropertyCount( ); ++i )
		{
			// Get property
			const MetaProperty* prop = cls->GetProperty( i );

			// Do not serialize if property is null or non-serializable
			if ( !prop || prop->HasFlags( MetaPropertyFlags::NonSerializeable ) )
			{
				continue;
			}

			// Serialize out the property
			PropertyArchiver::Serialize( object, prop, buffer );
		}

		return Result::SUCCESS;
	}

	//=====================================================================


	Result ObjectArchiver::Deserialize( const String& filePath, Vector< Object* >& out )
	{
		// Reset all data before de-serializing
		Reset( );

		// Read contents into buffer
		mBuffer.ReadFromFile( filePath ); 

		// Header information 
		const MetaClass* cls = Object::GetClass( mBuffer.Read< String >( ) );	// Read class type
		u32 versionNumber = mBuffer.Read< u32 >( );								// Read version number id

		if ( cls )
		{ 
			// Construct new object based on class
			Object* object = cls->Construct( );

			// Couldn't construct object
			if ( !object )
			{
				delete object;
				object = nullptr;
				return Result::FAILURE;
			}

			// Push back object into objects vector
			out.push_back( object );

			Result res = object->DeserializeData( &mBuffer );

			if ( res == Result::INCOMPLETE )
			{
				DeserializeObjectDataDefault( object, cls );
			} 
		}

		return Result::SUCCESS;
	} 

	//=====================================================================

	Object* ObjectArchiver::Deserialize( const String& filePath )
	{
		// Reset the buffer
		Reset( );

		// Read contents into buffer
		mBuffer.ReadFromFile( filePath );
 
		// Object Header information 
		const MetaClass* cls = Object::GetClass( mBuffer.Read< String >( ) );	// Read class type
		u32 versionNumber = mBuffer.Read< u32 >( );								// Read version number id

		// Object to construct and fill out
		Object* object = nullptr;

		if ( cls )
		{
			// Construct new object based on class
			object = cls->Construct( );

			// Couldn't construct object
			if ( !object )
			{
				delete object;
				object = nullptr;
				return nullptr;
			} 
			// Successfully constructed, now deserialize data into it
			else
			{
				Result res = object->DeserializeData( &mBuffer );

				// Default deserialization method if not object does not handle its own deserialization
				if ( res == Result::INCOMPLETE )
				{
					res = DeserializeObjectDataDefault( object, cls );
				} 

				// Delete object if not deserialized correctly
				if ( res != Result::SUCCESS )
				{
					delete object;
					object = nullptr;
				} 
				// Otherwise call late init after deserializing
				else
				{
					object->DeserializeLateInit( );
				}
			} 
		}

		// Return object, either null or filled out
		return object; 
	}

	Result ObjectArchiver::Deserialize( const String& filePath, Object* object )
	{
		// Reset the byte buffer
		Reset( );

		// Read contents into buffer
		mBuffer.ReadFromFile( filePath );

		if ( mBuffer.GetStatus( ) == BufferStatus::ReadyToRead )
		{
			// Return result from static method
			return Deserialize( &mBuffer, object ); 
		}
		else
		{
			return Result::FAILURE;
		}
	}

	//=====================================================================

	Result ObjectArchiver::Deserialize( const String& filePath, HashMap< const MetaClass*, Vector< Object* > >& out )
	{
		return Result::SUCCESS;
	}

	//=====================================================================

	/*
	*@brief Takes an existing byte buffer, parses the buffer and then fills out the object passed in using that buffer
	*/ 
	Result ObjectArchiver::Deserialize( ByteBuffer* buffer, Object* object )
	{
		// Object Header information 
		const MetaClass* cls = Object::GetClass( buffer->Read< String >( ) );	// Read class type
		u32 versionNumber = buffer->Read< u32 >( );								// Read version number id 

		if ( cls )
		{
			// If nullptr, then attempt to construct the object
			if ( object == nullptr )
			{
				object = cls->Construct( ); 

				// Couldn't construct object after attempting
				if ( !object )
				{
					delete object;
					object = nullptr;
					return Result::FAILURE;
				} 
			} 
			// Successfully constructed, now deserialize data into it
			else
			{
				Result res = object->DeserializeData( buffer );

				// Default deserialization method if not object does not handle its own deserialization
				if ( res == Result::INCOMPLETE )
				{
					res = DeserializeObjectDataDefault( object, cls, buffer );
				}

				// Delete object if not deserialized correctly
				if ( res != Result::SUCCESS )
				{
					delete object;
					object = nullptr;
				}
				// Otherwise call late init after deserializing
				else
				{
					object->DeserializeLateInit( );
				}
			}
		}

		// Return object, either null or filled out
		return Result::SUCCESS;
	}

	//=====================================================================

	Object* ObjectArchiver::Deserialize( ByteBuffer* buffer ) 
	{ 
		// Object Header information 
		const MetaClass* cls = Object::GetClass( buffer->Read< String >( ) );	// Read class type
		u32 versionNumber = buffer->Read< u32 >( );								// Read version number id

		// Object to construct and fill out
		Object* object = nullptr;

		if ( cls )
		{
			// Construct new object based on class
			object = cls->Construct( );

			// Couldn't construct object
			if ( !object )
			{
				delete object;
				object = nullptr;
				return nullptr;
			} 
			// Successfully constructed, now deserialize data into it
			else
			{
				Result res = object->DeserializeData( buffer );

				// Default deserialization method if not object does not handle its own deserialization
				if ( res == Result::INCOMPLETE )
				{
					res = DeserializeObjectDataDefault( object, cls, buffer );
				} 

				// Delete object if not deserialized correctly
				if ( res != Result::SUCCESS )
				{
					delete object;
					object = nullptr;
				}
				// Otherwise call late init after deserializing
				else
				{
					object->DeserializeLateInit( );
				}
			} 
		}

		// Return object, either null or filled out
		return object; 
	}

	//===================================================================== 

	Result ObjectArchiver::DeserializeObjectDataDefault( const Object* object, const MetaClass* cls )
	{
		return DeserializeObjectDataDefault( object, cls, &mBuffer );
	}

	//===========================================================

	Result ObjectArchiver::DeserializeObjectDataDefault( const Object* object, const MetaClass* cls, ByteBuffer* buffer )
	{ 
		// Read in property count of serializable properties
		usize propCount = buffer->Read< usize >( );

		for ( usize i = 0; i < propCount; ++i )
		{
			// Deserialize the property
			PropertyArchiver::Deserialize( object, buffer );
		}

		return Result::SUCCESS;
	}

	//===========================================================

	Result ObjectArchiver::WriteToFile( const String& filePath )
	{ 
		mBuffer.WriteToFile( filePath ); 
		return Result::SUCCESS;
	}

	//=====================================================================

	void ObjectArchiver::Reset( )
	{
		mBuffer.Reset( );
	} 

	//=====================================================================

	Result ObjectArchiver::MergeObjects( Object* source, Object* dest, MergeType mergeType )
	{
		const MetaClass* cls = source->Class();

		if ( !cls->InstanceOf( dest->Class() ) )
		{
		// Error, can't operate on separate types.
			return Result::FAILURE;
		} 

		// Attempt to do customized merging depending on the object itself
		Result mergeResult = dest->MergeWith( source, mergeType );

		// If incomplete operation, then default merging behavior
		if ( mergeResult == Result::INCOMPLETE )
		{
			MergeObjectsDefault( source, dest, mergeType );
		} 

		return Result::SUCCESS;
	}

	//=====================================================================

	Result ObjectArchiver::MergeObjectsDefault( Object* source, Object* dest, MergeType mergeType )
	{
		const MetaClass* cls = source->Class();

		for ( usize i = 0; i < cls->GetPropertyCount(); ++i )
		{
			const MetaProperty* prop = cls->GetProperty( i );

			// Maybe same as "IsSerializeable"? 
			if ( prop->IsSerializeable() )
			{
				switch( mergeType )
				{
					case MergeType::AcceptSource:
					{
						MergeProperty( source, dest, prop, mergeType );
					} break;

					case MergeType::AcceptDestination:
					{
						MergeProperty( dest, source, prop, mergeType );
					} break;

					case MergeType::AcceptMerge:
					{
						// Only merge iff destination object doesn't have a property override
						if ( !prop->HasOverride( dest ) )
						{
							MergeProperty( source, dest, prop, mergeType );
						}
					} break;
				}
			}
		} 

		return Result::SUCCESS;
	}

	//=====================================================================

	Result ObjectArchiver::MergeProperty( Object* source, Object* dest, const MetaProperty* prop, MergeType mergeType )
	{
		// Will merge the source object property into the destination property
		const MetaClass* cls = source->Class();
		
		switch ( prop->GetType() ) 
		{
			case MetaPropertyType::U32:
			{ 
				cls->SetValue( dest, prop, *cls->GetValueAs< u32 >( source, prop ) ); 
			} break;

			case MetaPropertyType::F32:
			{ 
				cls->SetValue( dest, prop, *cls->GetValueAs< f32 >( source, prop ) ); 
			} break;

			case MetaPropertyType::Vec3:
			{
				cls->SetValue( dest, prop, *cls->GetValueAs< Vec3 >( source, prop ) );
			} break;

			case MetaPropertyType::AssetHandle:
			{
				cls->SetValue( dest, prop, *cls->GetValueAs< AssetHandle< Asset > >( source, prop ) );
			} break;

			case MetaPropertyType::Object:
			{
				// If object is pointer
				if ( prop->GetTraits( ).IsPointer( ) )
				{
					const MetaPropertyPointerBase* base = prop->Cast< MetaPropertyPointerBase >( );
					Object* sourceObj = base->GetValueAsObject( source )->ConstCast< Object >( );
					Object* destObj = base->GetValueAsObject( dest )->ConstCast< Object >( );

					// Merge objects based on merge type
					ObjectArchiver::MergeObjects( sourceObj, destObj, mergeType );
				} 
				// Not pointer
				else
				{
					// Write out to temp to write size of object
					Object* sourceObj = cls->GetValueAs< Object >( source, prop )->ConstCast< Object >( );
					Object* destObj = cls->GetValueAs< Object >( dest, prop )->ConstCast< Object >( );

					// Merge objects based on merge type
					ObjectArchiver::MergeObjects( sourceObj, destObj, mergeType );
				} 
			} break;

			case MetaPropertyType::Array:
			{
				// Get base
				const MetaPropertyArrayBase* base = prop->Cast< MetaPropertyArrayBase >( ); 

				// Write out array elements
				switch ( base->GetArrayType( ) )
				{ 
					case MetaPropertyType::AssetHandle:
					{ 
						// Grab array property
						const MetaPropertyArray< AssetHandle< Asset > >* arrProp = base->Cast< MetaPropertyArray< AssetHandle< Asset > > >( );

						// Get sizes of arrays
						usize arrSize = arrProp->GetSize( source ) == arrProp->GetSize( dest ) ? arrProp->GetSize( source ) : 0; 

						// Write out asset uuids in array
						for ( usize j = 0; j < arrSize; ++j )
						{
							// Grab value from source
							AssetHandle<Asset> asset;
							arrProp->GetValueAt( source, j, &asset );
							// Set destination value
							arrProp->SetValueAt( dest, j, asset );
						}
					} break; 
				}
			} break;

			// Etc...
		}

		return Result::SUCCESS;
	}

	//===================================================================== 

	bool ObjectArchiver::HasPropertyOverrides( const Object* obj )
	{
		bool hasOverrides = false;

		// Attempt override function for collecting property overrides
		Result res = obj->HasPropertyOverrides( hasOverrides );

		// If incomplete result, then attempt default override collection
		if ( res == Result::INCOMPLETE )
		{
			hasOverrides = HasPropertyOverridesDefault( obj );
		}

		return hasOverrides;
	}

	//===================================================================== 

	bool ObjectArchiver::HasPropertyOverridesDefault( const Object* obj )
	{
		bool hasOverride = false;

		// Collect class
		const MetaClass* cls = obj->Class( );

		for ( usize i = 0; i < cls->GetPropertyCount( ); ++i )
		{
			const MetaProperty* prop = cls->GetProperty( i );

			// Add to has override result
			hasOverride |= prop->HasOverride( obj );
		}

		// Return final result
		return hasOverride;
	}

	//===================================================================== 

	Result ObjectArchiver::RecordAllPropertyOverrides( Object* source, Object* dest )
	{ 
		// Attempt default behavior first
		Result res = dest->RecordPropertyOverrides( source );
		if ( res == Result::INCOMPLETE )
		{
			// Default behavior
			res = RecordAllPropertyOverridesDefault( source, dest );
		} 

		return res;
	}

	//===================================================================== 

#define ENJON_RECORD_OVERRIDE_POD( cls, sourceObj, destObj, prop, podType )\
{\
	podType sourceVal = *cls->GetValueAs< podType >( sourceObj, prop );\
	podType destVal = *cls->GetValueAs< podType >( destObj, prop );\
\
	if ( sourceVal != destVal )\
	{\
		prop->AddOverride( destObj, sourceObj );\
	}\
}

	Result ObjectArchiver::RecordAllPropertyOverridesDefault( Object* source, Object* dest )
	{
		// Get source class
		const MetaClass* cls = source->Class( );

		if ( !source->Class( )->InstanceOf( dest->Class() ) )
		{
			// Error, cannot operate on separate types
			return Result::FAILURE;
		}

		for ( usize i = 0; i < cls->GetPropertyCount( ); ++i )
		{
			// Grab property from class
			MetaProperty* prop = const_cast< MetaProperty* >( cls->GetProperty( i ) );

			switch ( prop->GetType( ) )
			{
				case MetaPropertyType::S32: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, s32 ); break; 
				case MetaPropertyType::U32: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, u32 ); break; 
				case MetaPropertyType::F32: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, f32 ); break; 
				case MetaPropertyType::Vec2: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, Vec2 ); break; 
				case MetaPropertyType::Vec3: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, Vec3 ); break; 
				case MetaPropertyType::Vec4: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, Vec4 ); break; 
				case MetaPropertyType::Quat: ENJON_RECORD_OVERRIDE_POD( cls, source, dest, prop, Quaternion ); break; 

				case MetaPropertyType::Transform:
				{ 
					Transform* sourceTransform = cls->GetValueAs< Transform >( source, prop )->ConstCast< Transform >( );
					Transform* destTransform = cls->GetValueAs< Transform >( dest, prop )->ConstCast < Transform >( );

					// Record property overrides
					RecordAllPropertyOverrides( sourceTransform, destTransform ); 
				} break; 
			}
		} 

		return Result::SUCCESS;
	}

	//===================================================================== 

	Result ObjectArchiver::ClearAllPropertyOverrides( )
	{ 
		return Result::SUCCESS;
	}

	//===================================================================== 
}












