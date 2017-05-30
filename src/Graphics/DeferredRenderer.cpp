#include "Graphics/DeferredRenderer.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/GBuffer.h"
#include "Graphics/FullScreenQuad.h"
#include "Graphics/SpriteBatch.h"
#include "Graphics/FontManager.h"
#include "Graphics/ShaderManager.h"
#include "Graphics/Material.h"
#include "Graphics/Mesh.h"
#include "Graphics/Renderable.h"
#include "Graphics/Font.h"
#include "Graphics/FontManager.h"
#include "Graphics/Color.h"
#include "Graphics/DirectionalLight.h"
#include "Graphics/PointLight.h"
#include "Graphics/SpotLight.h"
#include "Graphics/Shader.h"
#include "Graphics/ShaderGraph.h"
#include "Graphics/ShaderGraph/ShaderMathNode.h"
#include "Graphics/ShaderGraph/ShaderVectorNode.h"
#include "IO/ResourceManager.h"
#include "Asset/AssetManager.h"
#include "Console.h"
#include "CVarsSystem.h"
#include "ImGui/ImGuiManager.h"
#include "Engine.h"

#include <string>
#include <fmt/format.h> 
#include <fmt/string.h>
#include <fmt/printf.h>
#include <cassert>

Enjon::Shader mShader;
Enjon::ShaderGraph mShaderGraph;
Enjon::Renderable mRenderable;

namespace Enjon 
{ 
	//======================================================================================================

	DeferredRenderer::DeferredRenderer()
	{
	}

	//======================================================================================================

	DeferredRenderer::~DeferredRenderer()
	{
	}

	//======================================================================================================

	Enjon::Result DeferredRenderer::Shutdown()
	{
		delete(mGbuffer);
		delete(mDebugTarget);
		delete(mSmallBlurHorizontal);
		delete(mSmallBlurVertical);
		delete(mMediumBlurHorizontal);
		delete(mMediumBlurVertical);
		delete(mLargeBlurHorizontal);
		delete(mLargeBlurVertical);
		delete(mCompositeTarget);
		delete(mLightingBuffer);
		delete(mLuminanceTarget);
		delete(mFXAATarget);
		delete(mShadowDepth);

		return Result::SUCCESS; 
	}

	//======================================================================================================

	Enjon::Result DeferredRenderer::Initialize()
	{
		// TODO(John): Need to have a way to have an .ini that's read or grab these values from a static
		// engine config file
		// mWindow.Init("Game", 1920, 1080, WindowFlagsMask((u32)WindowFlags::FULLSCREEN)); 
		mWindow.Init( "Game", 1440, 900, WindowFlags::RESIZABLE ); 

		// Initialize shader manager
		Enjon::ShaderManager::Init();

		// Initialize font manager
		Enjon::FontManager::Init();

		// Initialize scene camera
		mSceneCamera = Enjon::Camera(mWindow.GetViewport());
		mSceneCamera.SetProjection(ProjectionType::Perspective);
		mSceneCamera.SetPosition(Vec3(0, 5, 10));
		mSceneCamera.LookAt(Vec3(0, 0, 0));

		// Initialize frame buffers
		InitializeFrameBuffers();
		// Calcualte blur weights
		CalculateBlurWeights();
		// Register cvars
		RegisterCVars();

		GLSLProgram* shader = Enjon::ShaderManager::Get("GBuffer");
		shader->Use();
			shader->SetUniform("u_albedoMap", 0);
			shader->SetUniform("u_normalMap", 1);
		shader->Unuse();

		mShowGraphicsOptionsWindow = true;
		auto graphicsMenuOption = [&]()
		{
        	ImGui::MenuItem("Graphics##options", NULL, &mShowGraphicsOptionsWindow);
		};

		mShowGame = true;
		auto showGameViewportFunc = [&]()
		{
			// Docking windows
			if (ImGui::BeginDock("Game View", &mShowGame, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				// Print docking information
				ShowGameViewport(&mShowGame);
			}
			ImGui::EndDock();
		};

		auto showGraphicsViewportFunc = [&]()
		{
			// Docking windows
			if (ImGui::BeginDock("Graphics", &mShowGraphicsOptionsWindow))
			{
				// Print docking information
				ShowGraphicsWindow(&mShowGraphicsOptionsWindow);
			}
			ImGui::EndDock();
		};

	 	mShowStyles = true;
	 	auto showStylesWindowFunc = [&]()
	 	{
			if (ImGui::BeginDock("Styles##options", &mShowStyles))
			{
				ImGui::ShowStyleEditor();	
			}
			ImGui::EndDock();
	 	};

		// ImGuiManager::Register(graphicsMenuOption);
		// TODO(John): I HATE the way this looks
		ImGuiManager::RegisterMenuOption("View", graphicsMenuOption);
		ImGuiManager::RegisterWindow(showGameViewportFunc);
		ImGuiManager::RegisterWindow(showGraphicsViewportFunc);
		ImGuiManager::RegisterWindow(showStylesWindowFunc);

		// Set current render texture
		mCurrentRenderTexture = mFXAATarget->GetTexture();

		// Register docking layouts
	    ImGuiManager::RegisterDockingLayout(ImGui::DockingLayout("Game View", nullptr, ImGui::DockSlotType::Slot_Top, 1.0f));
	    ImGuiManager::RegisterDockingLayout(ImGui::DockingLayout("Graphics", nullptr, ImGui::DockSlotType::Slot_Right, 0.1f));
	    ImGuiManager::RegisterDockingLayout(ImGui::DockingLayout("Styles##options", nullptr, ImGui::DockSlotType::Slot_Bottom, 0.2f));

		// TODO(): I don't like random raw gl calls just lying around...
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);

		ShaderTest( );

		return Result::SUCCESS;
	}

	//======================================================================================================

	void DeferredRenderer::Update(const f32 dT)
	{ 
		static bool set = false;
		if ( !set )
		{
			Enjon::AssetManager* am = Engine::GetInstance( )->GetSubsystemCatalog( )->Get< Enjon::AssetManager >( );
			if ( am )
			{
				mRenderable.SetMesh( am->GetAsset< Enjon::Mesh >( "isoarpg.models.unit_sphere" ) ); 
				mRenderable.SetScale( 3.0f );
				mRenderable.SetPosition( Vec3( 0, 10, 0 ) );
			}
			set = true;
		}

		// Get input
		Enjon::Input* input = Engine::GetInstance( )->GetSubsystemCatalog( )->Get< Enjon::Input >( );
		if ( input )
		{
			// Recompile test shader
			if ( input->IsKeyPressed( Enjon::KeyCode::R ) )
			{
				std::cout << "Recompiling...\n";
				mShader.Recompile( );
			}
		}

		// Gbuffer pass
		GBufferPass();
		// Lighting pass
		LightingPass();
		// Luminance Pass
		LuminancePass();
		// Bloom pass
		BloomPass();
		// Composite Pass
		CompositePass(mLightingBuffer);
		// FXAA pass
		FXAAPass(mCompositeTarget);

		// Clear default buffer
		mWindow.Clear( );

		// ImGui pass
		if (true)
		{
			GuiPass();
		}

		else
		{
			auto program = Enjon::ShaderManager::Get("NoCameraProjection");	
			program->Use();
			{
				mBatch->Begin();
				{
					mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1),
									mFXAATarget->GetTexture()
								);
				}
				mBatch->End();
				mBatch->RenderBatch();
			}
			program->Unuse();
		}

		mWindow.SwapBuffer();
	}

	//======================================================================================================

	void DeferredRenderer::GBufferPass()
	{
		// Bind gbuffer
		mGbuffer->Bind();

		GLSLProgram* shader = Enjon::ShaderManager::Get("GBuffer");

		// Use gbuffer shader
		shader->Use();

		// Clear buffer (default)
		glClearBufferfv(GL_COLOR, 0, mBGColor);

		/*
		GLfloat black[] = {0.0f, 0.0f, 0.0f, 1.0f};
		for (u32 i = 1; i < (u32)GBufferTextureType::GBUFFER_TEXTURE_COUNT; ++i)
		{
			glClearBufferfv(GL_COLOR, i, black);
		}
		*/

		glClear(GL_DEPTH_BUFFER_BIT);

		// mWindow.Clear(1.0f, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, RGBA16_LightGrey());

		// Get sorted renderables by material
		std::vector<Renderable*> sortedRenderables = mScene.GetRenderables();
		std::set<QuadBatch*>* sortedQuadBatches = mScene.GetQuadBatches();

		if (!sortedRenderables.empty())
		{
			// Set set shared uniform
			shader->SetUniform("u_camera", mSceneCamera.GetViewProjection());

			Material* material = nullptr;
			for (auto& renderable : sortedRenderables)
			{
				// Check for material switch
				Material* curMaterial = renderable->GetMaterial();
				assert(curMaterial != nullptr);
				if (material != curMaterial)
				{
					// Set material
					material = curMaterial;

					// Set material textures
					shader->BindTexture("u_albedoMap", material->GetTexture(Enjon::TextureSlotType::Albedo).Get()->GetTextureId(), 0);
					shader->BindTexture("u_normalMap", material->GetTexture(Enjon::TextureSlotType::Normal).Get()->GetTextureId(), 1);
					shader->BindTexture("u_emissiveMap", material->GetTexture(Enjon::TextureSlotType::Emissive).Get()->GetTextureId(), 2);
					shader->BindTexture("u_metallicMap", material->GetTexture(Enjon::TextureSlotType::Metallic).Get()->GetTextureId(), 3);
					shader->BindTexture("u_roughnessMap", material->GetTexture(Enjon::TextureSlotType::Roughness).Get()->GetTextureId(), 4);
					shader->BindTexture("u_aoMap", material->GetTexture(Enjon::TextureSlotType::AO).Get()->GetTextureId(), 5);
					shader->SetUniform("u_albedoColor", material->GetColor(Enjon::TextureSlotType::Albedo));
				}

				// Now need to render
				Mesh* mesh = renderable->GetMesh().Get();
				mesh->Bind();
				{
					Mat4 Model;
					Model *= Mat4::Translate(renderable->GetPosition());
					Model *= QuaternionToMat4(renderable->GetRotation());
					Model *= Mat4::Scale(renderable->GetScale());
					shader->SetUniform("u_model", Model);
					mesh->Submit();
				}
				mesh->Unbind();
			}
		}

		// Unuse gbuffer shader
		shader->Unuse();

		// Quadbatches
		shader = Enjon::ShaderManager::Get("QuadBatch");
		shader->Use();

		if (!sortedQuadBatches->empty())
		{
			// Set shared uniform
			shader->SetUniform("u_camera", mSceneCamera.GetViewProjection());

			Material* material = nullptr;
			for (auto& quadBatch : *sortedQuadBatches)
			{
				Material* curMaterial = quadBatch->GetMaterial();
				assert(curMaterial != nullptr);
				if (material != curMaterial)
				{ 
					// Set material
					material = curMaterial;
					
					// Check whether or not to be rendered two sided
					if ( material->TwoSided( ) )
					{
						glDisable( GL_CULL_FACE );
					}
					else
					{
						glEnable( GL_CULL_FACE );
						glCullFace( GL_BACK );
					}

					// Set material tetxures
					shader->BindTexture("u_albedoMap", material->GetTexture(Enjon::TextureSlotType::Albedo).Get()->GetTextureId(), 0);
					shader->BindTexture("u_normalMap", material->GetTexture(Enjon::TextureSlotType::Normal).Get()->GetTextureId(), 1);
					shader->BindTexture("u_emissiveMap", material->GetTexture(Enjon::TextureSlotType::Emissive).Get()->GetTextureId(), 2);
					shader->BindTexture("u_metallicMap", material->GetTexture(Enjon::TextureSlotType::Metallic).Get()->GetTextureId(), 3);
					shader->BindTexture("u_roughnessMap", material->GetTexture(Enjon::TextureSlotType::Roughness).Get()->GetTextureId(), 4);
					shader->BindTexture("u_aoMap", material->GetTexture(Enjon::TextureSlotType::AO).Get()->GetTextureId(), 5);
				}

				// Render batch
				quadBatch->RenderBatch();
			}
		}

		// Unuse quadbatch shader
		shader->Unuse();

		// Now do the shader test
		mShader.Use( );
		{
			static bool set = false;
			Enjon::AssetManager* am = Engine::GetInstance( )->GetSubsystemCatalog( )->Get< Enjon::AssetManager >( );
			static Enjon::AssetHandle< Enjon::Texture > albedo;
			static Enjon::AssetHandle< Enjon::Texture > normals;
			static Enjon::AssetHandle< Enjon::Texture > metallic;
			static Enjon::AssetHandle< Enjon::Texture > roughness;
			static Enjon::AssetHandle< Enjon::Texture > emissive; 
			
			if ( !set )
			{
				albedo = am->GetAsset< Enjon::Texture >( "isoarpg.materials.cerebus.albedo" );
				normals = am->GetAsset< Enjon::Texture >( "isoarpg.materials.cerebus.normal" );
				metallic = am->GetAsset< Enjon::Texture >( "isoarpg.materials.cerebus.metallic" );
				roughness = am->GetAsset< Enjon::Texture >( "isoarpg.materials.cerebus.roughness" );
				emissive = am->GetAsset< Enjon::Texture >( "isoarpg.materials.cerebus.emissive" ); 
				set = true;
			}
			// Set textures
			mShader.BindTexture( "uAlbedoMap", albedo.Get( )->GetTextureId( ), 0 );
			mShader.BindTexture( "uEmissiveMap", emissive.Get()->GetTextureId( ), 1 );
			mShader.BindTexture( "uMetallicMap", metallic.Get()->GetTextureId( ), 2 );
			mShader.BindTexture( "uNormalMap", normals.Get()->GetTextureId( ), 3 );
			mShader.BindTexture( "uRoughnessMap", roughness.Get()->GetTextureId( ), 4 );

			static f32 t = 0.0f;
			t += 0.001f;

			// Set uniforms
			mShader.SetUniform( "uCamera", mSceneCamera.GetViewProjectionMatrix( ) );
			mShader.SetUniform( "uWorldTime", t );

			Mesh* mesh = mRenderable.GetMesh( ).Get( );
			mesh->Bind( );
			{
				Mat4 Model;
				Model *= Mat4::Translate( mRenderable.GetPosition( ) );
				Model *= QuaternionToMat4( mRenderable.GetRotation( ) );
				Model *= Mat4::Scale( mRenderable.GetScale( ) );
				mShader.SetUniform( "uModel", Model );
				mesh->Submit( );
			}
			mesh->Unbind( ); 
		}
		mShader.Unuse( );

		// Unbind gbuffer
		mGbuffer->Unbind();
	}

	//======================================================================================================

	void DeferredRenderer::LightingPass()
	{
		mLightingBuffer->Bind();
		// mFullScreenQuad->Bind();
		
		GLSLProgram* ambientShader 		= Enjon::ShaderManager::Get("AmbientLight");
		GLSLProgram* directionalShader 	= Enjon::ShaderManager::Get("PBRDirectionalLight");	
		GLSLProgram* pointShader 		= Enjon::ShaderManager::Get("PBRPointLight");	
		GLSLProgram* spotShader 		= Enjon::ShaderManager::Get("SpotLight");	

		std::set<DirectionalLight*>* directionalLights 	= mScene.GetDirectionalLights();	
		std::set<SpotLight*>* spotLights 				= mScene.GetSpotLights();	
		std::set<PointLight*>* pointLights 				= mScene.GetPointLights();

		AmbientSettings* aS = mScene.GetAmbientSettings();

		mWindow.Clear();

		// TODO(): Abstract these away 
		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_ONE, GL_ONE);

		// Ambient pass
		ambientShader->Use();
		{
			ambientShader->BindTexture("u_albedoMap", mGbuffer->GetTexture(GBufferTextureType::ALBEDO), 0);
			ambientShader->BindTexture("u_emissiveMap", mGbuffer->GetTexture(GBufferTextureType::EMISSIVE), 1);
			ambientShader->SetUniform("u_ambientColor", Vec3(aS->mColor.r, aS->mColor.g, aS->mColor.b));
			ambientShader->SetUniform("u_ambientIntensity", aS->mIntensity);
			ambientShader->SetUniform("u_resolution", mGbuffer->GetResolution());

			// Render
				mBatch->Begin();
					mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1),
									mGbuffer->GetTexture(GBufferTextureType::ALBEDO)
								);
				mBatch->End();
				mBatch->RenderBatch();
		}
		ambientShader->Unuse();

		directionalShader->Use();
		{
			directionalShader->SetUniform("u_camPos", mSceneCamera.GetPosition() + mSceneCamera.Backward());
			for (auto& l : *directionalLights)
			{
				ColorRGBA16 color = l->GetColor();

				directionalShader->BindTexture("u_albedoMap", 		mGbuffer->GetTexture(GBufferTextureType::ALBEDO), 0);
				directionalShader->BindTexture("u_normalMap", 		mGbuffer->GetTexture(GBufferTextureType::NORMAL), 1);
				directionalShader->BindTexture("u_positionMap", 	mGbuffer->GetTexture(GBufferTextureType::POSITION), 2);
				directionalShader->BindTexture("u_matProps", 		mGbuffer->GetTexture(GBufferTextureType::MAT_PROPS), 3);
				// directionalShader->BindTexture("u_shadowMap", 		mShadowDepth->GetDepth(), 4);
				directionalShader->SetUniform("u_resolution", 		mGbuffer->GetResolution());
				// directionalShader->SetUniform("u_lightSpaceMatrix", mShadowCamera->GetViewProjectionMatrix());
				// directionalShader->SetUniform("u_shadowBias", 		EM::Vec2(0.005f, ShadowBiasMax));
				directionalShader->SetUniform("u_lightDirection", 	l->GetDirection());															
				directionalShader->SetUniform("u_lightColor", 		Vec3(color.r, color.g, color.b));
				directionalShader->SetUniform("u_lightIntensity", 	l->GetIntensity());

				// Render	
				// TODO(): Fix full screen quad - super janky
				// mFullScreenQuad->Submit();
				mBatch->Begin();
					mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1),
									mGbuffer->GetTexture(GBufferTextureType::ALBEDO)
								);
				mBatch->End();
				mBatch->RenderBatch();
			}
		}
		directionalShader->Unuse();

		pointShader->Use();
		{
			pointShader->BindTexture("u_albedoMap", mGbuffer->GetTexture(GBufferTextureType::ALBEDO), 0);
			pointShader->BindTexture("u_normalMap", mGbuffer->GetTexture(GBufferTextureType::NORMAL), 1);
			pointShader->BindTexture("u_positionMap", mGbuffer->GetTexture(GBufferTextureType::POSITION), 2);
			pointShader->BindTexture("u_matProps", mGbuffer->GetTexture(GBufferTextureType::MAT_PROPS), 3);
			pointShader->SetUniform("u_resolution", mGbuffer->GetResolution());
			pointShader->SetUniform("u_camPos", mSceneCamera.GetPosition() + mSceneCamera.Backward());			

			for (auto& l : *pointLights)
			{
				ColorRGBA16& color = l->GetColor();
				Vec3& position = l->GetPosition();

				pointShader->SetUniform("u_lightPos", position);
				pointShader->SetUniform("u_lightColor", Vec3(color.r, color.g, color.b));
				pointShader->SetUniform("u_lightIntensity", l->GetIntensity());
				pointShader->SetUniform("u_attenuationRate", l->GetAttenuationRate());
				pointShader->SetUniform("u_radius", l->GetRadius());

				// Render Light to screen
				mBatch->Begin();
					mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1),
									mGbuffer->GetTexture(GBufferTextureType::ALBEDO)
								);
				mBatch->End();
				mBatch->RenderBatch();
			}
		}
		pointShader->Unuse();

		spotShader->Use();
		{
			for (auto& l : *spotLights)
			{
				spotShader->BindTexture("u_albedoMap", mGbuffer->GetTexture(GBufferTextureType::ALBEDO), 0);
				spotShader->BindTexture("u_normalMap", mGbuffer->GetTexture(GBufferTextureType::NORMAL), 1);
				spotShader->BindTexture("u_positionMap", mGbuffer->GetTexture(GBufferTextureType::POSITION), 2);
				// spotShader->BindTexture("u_matProps", mGbuffer->GetTexture(GBufferTextureType::MAT_PROPS), 3);
				spotShader->SetUniform("u_resolution", mGbuffer->GetResolution());
				spotShader->SetUniform("u_camPos", mSceneCamera.GetPosition());			

				for (auto& l : *spotLights)
				{
					ColorRGBA16& color = l->GetColor();
					SLParams& params = l->GetParams();
					Vec3& position = l->GetPosition();

					spotShader->SetUniform("u_lightPos", position);
					spotShader->SetUniform("u_lightColor", Vec3(color.r, color.g, color.b));
					spotShader->SetUniform("u_falloff", params.mFalloff);
					spotShader->SetUniform("u_lightIntensity", l->GetIntensity());
					spotShader->SetUniform("u_lightDirection", params.mDirection);
					spotShader->SetUniform("u_innerCutoff", params.mInnerCutoff);
					spotShader->SetUniform("u_outerCutoff", params.mOuterCutoff);

					// Render Light to screen
					mBatch->Begin();
						mBatch->Add(
										Vec4(-1, -1, 2, 2),
										Vec4(0, 0, 1, 1),
										mGbuffer->GetTexture(GBufferTextureType::ALBEDO)
									);
					mBatch->End();
					mBatch->RenderBatch();
				}
			}
		}
		spotShader->Unuse();

		// mFullScreenQuad->Unbind();
		mLightingBuffer->Unbind();	

		glEnable(GL_DEPTH_TEST);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	//======================================================================================================

	void DeferredRenderer::LuminancePass()
	{
		GLSLProgram* luminanceProgram = Enjon::ShaderManager::Get("Bright");
		mLuminanceTarget->Bind();
		{
			mWindow.Clear(1.0f, GL_COLOR_BUFFER_BIT, RGBA16_Black());
			luminanceProgram->Use();
			{
				luminanceProgram->BindTexture("u_emissiveMap", mGbuffer->GetTexture(GBufferTextureType::EMISSIVE), 1);
				luminanceProgram->SetUniform("u_threshold", mToneMapSettings.mThreshold);
				mBatch->Begin();
				{
					mBatch->Add(
										Vec4(-1, -1, 2, 2),
										Vec4(0, 0, 1, 1),
										mLightingBuffer->GetTexture()
									);
				}
				mBatch->End();
				mBatch->RenderBatch();
			}
			luminanceProgram->Unuse();
		}
		mLuminanceTarget->Unbind();
	}

	//======================================================================================================

	void DeferredRenderer::BloomPass()
	{
		GLSLProgram* horizontalBlurProgram = Enjon::ShaderManager::Get("HorizontalBlur");
		GLSLProgram* verticalBlurProgram = Enjon::ShaderManager::Get("VerticalBlur");

		glEnable(GL_BLEND);
		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_ONE, GL_ONE);

		// Small blur
    	for (u32 i = 0; i < (u32)mBloomSettings.mIterations.x * 2; ++i)
    	{
    		bool isEven = (i % 2 == 0);
    		RenderTarget* target = isEven ? mSmallBlurHorizontal : mSmallBlurVertical;
    		GLSLProgram* program = isEven ? horizontalBlurProgram : verticalBlurProgram;

			target->Bind(RenderTarget::BindType::WRITE);
			{
				program->Use();
				{
					for (uint32_t j = 0; j < 16; j++)
					{
						std::string uniform = "u_blurWeights[" + std::to_string(j) + "]";
						program->SetUniform(uniform, mBloomSettings.mSmallGaussianCurve[j]);
					}

					program->SetUniform("u_weight", mBloomSettings.mWeights.x);
					program->SetUniform("u_blurRadius", mBloomSettings.mRadius.x);
					GLuint texID = i == 0 ? mLuminanceTarget->GetTexture() : isEven ? mSmallBlurVertical->GetTexture() : mSmallBlurHorizontal->GetTexture();
					mBatch->Begin();
					{
			    		mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1), 
									texID
								);
					}
					mBatch->End();
					mBatch->RenderBatch();
				}
				program->Unuse();
			}	
			target->Unbind();
    	}

		// Medium blur
    	for (u32 i = 0; i < (u32)mBloomSettings.mIterations.y * 2; ++i)
    	{
    		bool isEven = (i % 2 == 0);
    		RenderTarget* target = isEven ? mMediumBlurHorizontal : mMediumBlurVertical;
    		GLSLProgram* program = isEven ? horizontalBlurProgram : verticalBlurProgram;

			target->Bind(RenderTarget::BindType::WRITE);
			{
				program->Use();
				{
					for (uint32_t j = 0; j < 16; j++)
					{
						std::string uniform = "u_blurWeights[" + std::to_string(j) + "]";
						program->SetUniform(uniform, mBloomSettings.mMediumGaussianCurve[j]);
					}

					program->SetUniform("u_weight", mBloomSettings.mWeights.y);
					program->SetUniform("u_blurRadius", mBloomSettings.mRadius.y);
					GLuint texID = i == 0 ? mSmallBlurVertical->GetTexture() : isEven ? mMediumBlurVertical->GetTexture() : mMediumBlurHorizontal->GetTexture();
					//GLuint texID = i == 0 ? mLuminanceTarget->GetTexture() : isEven ? mMediumBlurVertical->GetTexture() : mMediumBlurHorizontal->GetTexture();
					mBatch->Begin();
					{
			    		mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1), 
									texID
								);
					}
					mBatch->End();
					mBatch->RenderBatch();
				}
				program->Unuse();
			}	
			target->Unbind();
    	}

		// Large blur
    	for (u32 i = 0; i < (u32)mBloomSettings.mIterations.z * 2; ++i)
    	{
    		bool isEven = (i % 2 == 0);
    		RenderTarget* target = isEven ? mLargeBlurHorizontal : mLargeBlurVertical;
    		GLSLProgram* program = isEven ? horizontalBlurProgram : verticalBlurProgram;

			target->Bind(RenderTarget::BindType::WRITE);
			{
				program->Use();
				{
					for (uint32_t j = 0; j < 16; j++)
					{
						std::string uniform = "u_blurWeights[" + std::to_string(j) + "]";
						program->SetUniform(uniform, mBloomSettings.mLargeGaussianCurve[j]);
					}

					program->SetUniform("u_weight", mBloomSettings.mWeights.z);
					program->SetUniform("u_blurRadius", mBloomSettings.mRadius.z);
					GLuint texID = i == 0 ? mMediumBlurVertical->GetTexture() : isEven ? mLargeBlurVertical->GetTexture() : mLargeBlurHorizontal->GetTexture();
					//GLuint texID = i == 0 ? mLuminanceTarget->GetTexture() : isEven ? mLargeBlurVertical->GetTexture() : mLargeBlurHorizontal->GetTexture();
					mBatch->Begin();
					{
			    		mBatch->Add(
									Vec4(-1, -1, 2, 2),
									Vec4(0, 0, 1, 1), 
									texID
								);
					}
					mBatch->End();
					mBatch->RenderBatch();
				}
				program->Unuse();
			}	
			target->Unbind();
    	}

		glEnable(GL_DEPTH_TEST);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	//======================================================================================================

	void DeferredRenderer::BloomPass2( )
	{
		GLSLProgram* horizontalBlurProgram = Enjon::ShaderManager::Get( "HorizontalBlur" );
		GLSLProgram* verticalBlurProgram = Enjon::ShaderManager::Get( "VerticalBlur" );

		glEnable( GL_BLEND );
		glDisable( GL_DEPTH_TEST );
		glBlendFunc( GL_ONE, GL_ONE ); 
	}

	//======================================================================================================

	void DeferredRenderer::FXAAPass(RenderTarget* input)
	{
		GLSLProgram* fxaaProgram = Enjon::ShaderManager::Get("FXAA");
		mFXAATarget->Bind();
		{
			mWindow.Clear();
			fxaaProgram->Use();
			{
				auto viewPort = GetViewport();
				fxaaProgram->SetUniform("u_resolution", Vec2(viewPort.x, viewPort.y));
				fxaaProgram->SetUniform("u_FXAASettings", Vec3(mFXAASettings.mSpanMax, mFXAASettings.mReduceMul, mFXAASettings.mReduceMin));
				mBatch->Begin();
				{
					mBatch->Add(
										Vec4(-1, -1, 2, 2),
										Vec4(0, 0, 1, 1),
										input->GetTexture()
									);
				}
				mBatch->End();
				mBatch->RenderBatch();
			}
			fxaaProgram->Unuse();
		}
		mFXAATarget->Unbind();
	}

	//======================================================================================================

	void DeferredRenderer::CompositePass(RenderTarget* input)
	{
		GLSLProgram* compositeProgram = Enjon::ShaderManager::Get("Composite");
		mCompositeTarget->Bind();
		{
			mWindow.Clear();
			compositeProgram->Use();
			{
				compositeProgram->BindTexture("u_blurTexSmall", mSmallBlurVertical->GetTexture(), 1);
				compositeProgram->BindTexture("u_blurTexMedium", mMediumBlurVertical->GetTexture(), 2);
				compositeProgram->BindTexture("u_blurTexLarge", mLargeBlurVertical->GetTexture(), 3);
				compositeProgram->SetUniform("u_exposure", mToneMapSettings.mExposure);
				compositeProgram->SetUniform("u_gamma", mToneMapSettings.mGamma);
				compositeProgram->SetUniform("u_bloomScalar", mToneMapSettings.mBloomScalar);
				compositeProgram->SetUniform("u_saturation", mToneMapSettings.mSaturation);
				mBatch->Begin();
				{
					mBatch->Add(
										Vec4(-1, -1, 2, 2),
										Vec4(0, 0, 1, 1),
										input->GetTexture()
									);
				}
				mBatch->End();
				mBatch->RenderBatch();
			}
			compositeProgram->Unuse();
		}
		mCompositeTarget->Unbind();
	}

	//======================================================================================================

	void DeferredRenderer::GuiPass()
	{
		static bool show_test_window = false;
		static bool show_frame_rate = false;
		static bool show_graphics_window = true;
		static bool show_app_layout = false;
		static bool show_game_viewport = true;

        // Queue up gui
        ImGuiManager::Render(mWindow.GetSDLWindow());
		 //ImGuiManager::RenderGameUI(&mWindow, mSceneCamera.GetView().elements, mSceneCamera.GetProjection().elements);

        // Flush
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        ImGui::Render();

		// Do some other random gui shit here to see if it still works
		GLSLProgram* textProgram = ShaderManager::Get( "Text" );
 
		iVec2 viewport = GetViewport( ); 
		Mat4 ortho = Mat4::Orthographic(0.0f, (f32)viewport.x, 0.0f, (f32)viewport.y, -1, 1);

		static bool hasMadeFont = false;
		static UIFont* font = nullptr;
		if ( !hasMadeFont )
		{
			// Add font to database
			Enjon::AssetManager* am = Engine::GetInstance( )->GetSubsystemCatalog( )->Get< Enjon::AssetManager >( ); 
			Enjon::String rootPath = Enjon::Engine::GetInstance()->GetConfig( ).GetRoot( );
			Enjon::String fontPath = rootPath + "/Assets/Fonts/Hack3d/Hack3d.ttf";
			am->AddToDatabase( fontPath , false );
			font = am->GetAsset< Enjon::UIFont >( "e.development.enjon.assets.fonts.hack3d.hack3d" ).Get( );

			hasMadeFont = true;
		}

		/*
		textProgram->Use( );
		{
			textProgram->SetUniform( "projection", ortho );
			mBatch->Begin( GlyphSortType::FRONT_TO_BACK );
			{ 
				Enjon::String str = "HELLO, BITCHES"; 

				f32 x = (f32) viewport.x / 2.0f;
				f32 y = (f32) viewport.y / 2.0f;
				Enjon::FontAtlas* atlas = font->GetAtlas( 64 ); 
				f32 textureWidth = atlas->GetAtlasTexture( ).Get( )->GetWidth( );
				f32 textureHeight = atlas->GetAtlasTexture( ).Get( )->GetHeight( );

				std::string::const_iterator c;
				for ( c = str.begin( ); c != str.end( ); c++ )
				{ 
					Enjon::FontGlyph glyph = atlas->GetGlyph( *c );
					f32 width = glyph.GetWidth( );
					f32 height = glyph.GetHeight( ); 

					f32 u1 = glyph.GetUVOffsetX( );
					f32 v1 = 1.0f - glyph.GetUVOffsetY( );
					f32 u2 = u1 + ( width / textureWidth );
					f32 v2 = v1 -( height / textureHeight );

					f32 w = width;
					f32 h = height; 
					
					Vec2 bearing = glyph.GetBearing( );
					GLfloat xpos = x + bearing.x;
					GLfloat ypos = y - (height - bearing.y); 

					Vec4 uv( u1, v2, u2, v1 );

					Enjon::AssetHandle< Enjon::Texture > atlasTexture = atlas->GetAtlasTexture( );
					mBatch->Add( Vec4( xpos, ypos, w, h ), uv, atlasTexture.Get( )->GetTextureId( ), Enjon::RGBA16_BurntOrange( ) ); 
	        
					s32 advance = glyph.GetAdvance( );
					x += (f32)advance;
				} 

			}
			mBatch->End( ); 
			mBatch->RenderBatch( );
		}
		textProgram->Unuse( );
		*/
	}

	//======================================================================================================

	void DeferredRenderer::SetViewport(iVec2& dimensions)
	{
		mWindow.SetViewport(dimensions);
	}

	//======================================================================================================

	iVec2 DeferredRenderer::GetViewport()
	{
		return mWindow.GetViewport();
	}

	//======================================================================================================

	f64 NormalPDF(const f64 x, const f64 s, f64 m = 0.0)
	{
		static const f64 inv_sqrt_2pi = 0.3989422804014327;
		f64 a = (x - m) / s;

		return inv_sqrt_2pi / s * std::exp(-0.5 * a * a);
	}

	//======================================================================================================

	void DeferredRenderer::InitializeFrameBuffers()
	{
		auto viewport = mWindow.GetViewport();
		Enjon::u32 width = (Enjon::u32)viewport.x;
		Enjon::u32 height = (Enjon::u32)viewport.y;

		mGbuffer 					= new GBuffer(width, height);
		mDebugTarget 				= new RenderTarget(width, height);
		mSmallBlurHorizontal 		= new RenderTarget(width / 4, height / 4);
		mSmallBlurVertical 			= new RenderTarget(width / 4, height / 4);
		mMediumBlurHorizontal 		= new RenderTarget(width  / 8, height  / 8);
		mMediumBlurVertical 		= new RenderTarget(width  / 8, height  / 8);
		mLargeBlurHorizontal 		= new RenderTarget(width / 16, height / 16);
		mLargeBlurVertical 			= new RenderTarget(width / 16, height / 16);
		mCompositeTarget 			= new RenderTarget(width, height);
		mLightingBuffer 			= new RenderTarget(width, height);
		mLuminanceTarget 			= new RenderTarget(width / 2, height / 2);
		mFXAATarget 				= new RenderTarget(width, height);
		mShadowDepth 				= new RenderTarget(2048, 2048);
		mFinalTarget 				= new RenderTarget(width, height);

		mBloomDownSampleHalfVertical			= new RenderTarget( width / 2, height / 2 );
		mBloomDownSampleHalfHorizontal			= new RenderTarget( width / 2, height / 2 );
		mBloomDownSampleQuarterVertical			= new RenderTarget( width / 4, height / 4 );
		mBloomDownSampleQuarterHorizontal		= new RenderTarget( width / 4, height / 4 );
		mBloomDownSampleEighthVertical			= new RenderTarget( width / 8, height / 8 );
		mBloomDownSampleEighthHorizontal		= new RenderTarget( width / 8, height / 8 );
		mBloomDownSampleSixteenthVertical		= new RenderTarget( width / 16, height / 16 );
		mBloomDownSampleSixteenthHorizontal		= new RenderTarget( width / 16, height / 16 );
		mBloomDownSampleThirtySecondVertical	= new RenderTarget( width / 32, height / 32 );
		mBloomDownSampleThirtySecondHorizontal	= new RenderTarget( width / 32, height / 32 );
		mBloomDownSampleSixtyFourthVertical		= new RenderTarget( width / 64, height / 64 );
		mBloomDownSampleSixtyFourthHorizontal	= new RenderTarget( width / 64, height / 64 );
		mBloomUpSampleHalfVertical				= new RenderTarget( width / 2, height / 2 );
		mBloomUpSampleHalfHorizontal			= new RenderTarget( width / 2, height / 2 );
		mBloomUpSampleQuarterVertical			= new RenderTarget( width / 4, height / 4 );
		mBloomUpSampleQuarterHorizontal			= new RenderTarget( width / 4, height / 4 );
		mBloomUpSampleEighthVertical			= new RenderTarget( width / 8, height / 8 );
		mBloomUpSampleEighthHorizontal			= new RenderTarget( width / 8, height / 8 );
		mBloomUpSampleSixteenthVertical			= new RenderTarget( width / 16, height / 16 );
		mBloomUpSampleSixteenthHorizontal		= new RenderTarget( width / 16, height / 16 );
		mBloomUpSampleThirtySecondVertical		= new RenderTarget( width / 32, height / 32 );
		mBloomUpSampleThirtySecondHorizontal	= new RenderTarget( width / 32, height / 32 );
		mBloomUpSampleSixtyFourthVertical		= new RenderTarget( width / 64, height / 64 );
		mBloomUpSampleSixtyFourthHorizontal		= new RenderTarget( width / 64, height / 64 );

		mBatch 						= new SpriteBatch();
		mBatch->Init();

		mFullScreenQuad 			= new FullScreenQuad();
	}

	//======================================================================================================

	void DeferredRenderer::CalculateBlurWeights()
	{
		f64 weight;
		f64 start = -3.0;
		f64 end = 3.0;
		f64 denom = 2.0 * end + 1.0;
		f64 num_samples = 15.0;
		f64 range = end * 2.0;
		f64 step = range / num_samples;
		u32 i = 0;

		weight = 1.74;
		for (f64 x = start; x <= end; x += step)
		{
			f64 pdf = NormalPDF(x, 0.23);
			mBloomSettings.mSmallGaussianCurve[i++] = pdf;
		}

		i = 0;
		weight = 3.9f;
		for (f64 x = start; x <= end; x += step)
		{
			f64 pdf = NormalPDF(x, 0.775);
			mBloomSettings.mMediumGaussianCurve[i++]= pdf;
		}

		i = 0;
		weight = 2.53f;
		for (f64 x = start; x <= end; x += step)
		{
			f64 pdf = NormalPDF(x, 1.0);
			mBloomSettings.mLargeGaussianCurve[i++] = pdf;
		}
	}

	//======================================================================================================

	void DeferredRenderer::RegisterCVars()
	{
		Enjon::CVarsSystem::Register("exposure", &mToneMapSettings.mExposure, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("gamma", &mToneMapSettings.mGamma, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("bloomScale", &mToneMapSettings.mBloomScalar, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("saturation", &mToneMapSettings.mSaturation, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_weight_small", &mBloomSettings.mWeights.x, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_weight_medium", &mBloomSettings.mWeights.y, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_weight_large", &mBloomSettings.mWeights.z, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_iter_small", &mBloomSettings.mIterations.x, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_iter_medium", &mBloomSettings.mIterations.y, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_iter_large", &mBloomSettings.mIterations.z, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("threshold", &mToneMapSettings.mThreshold, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_radius_small", &mBloomSettings.mRadius.x, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_radius_medium", &mBloomSettings.mRadius.y, Enjon::CVarType::TYPE_FLOAT);
		Enjon::CVarsSystem::Register("blur_radius_large", &mBloomSettings.mRadius.z, Enjon::CVarType::TYPE_FLOAT);
	}

	//======================================================================================================

	void DeferredRenderer::ShowGraphicsWindow(bool* p_open)
	{
	    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.65f);                                 // Right align, keep 140 pixels for labels

        ImGui::PushStyleColor(ImGuiCol_Text, ImColor(1.0, 0.6f, 0.0f, 1.0f));
        ImGui::Text("Graphics Options");
        ImGui::PopStyleColor(1);
        ImGui::Separator();

		ImGui::SliderFloat( "X", &uvs.x, 0.0f, 1.0f );
		ImGui::SliderFloat( "Y", &uvs.y, 0.0f, 1.0f );
		ImGui::SliderFloat( "Z", &uvs.z, 0.0f, 1.0f );
		ImGui::SliderFloat( "W", &uvs.w, 0.0f, 1.0f );

	    if (ImGui::TreeNode("ToneMapping"))
	    {
		    if (ImGui::TreeNode("Bloom"))
		    {
		    	if (ImGui::TreeNode("Blur"))
		    	{
				    if (ImGui::TreeNode("Small##bloom"))
				    {
				    	// Iterations
				    	s32 smallIter = (s32)mBloomSettings.mIterations.x;
					    ImGui::SliderInt("Iterations##small", &smallIter, 0, 30);
					    mBloomSettings.mIterations.x = smallIter;

					    // Radius
					    f32 radius = (f32)mBloomSettings.mRadius.x;
					    ImGui::SliderFloat("Radius##bloomsmall", &radius, 0.001f, 0.1f, "%.3f");
					    mBloomSettings.mRadius.x = radius;

					    // Weight
					    f32 weight = (f32)mBloomSettings.mWeights.x;
					    ImGui::SliderFloat("Weight##bloomsmall", &weight, 0.001, 0.5f, "%.3f");
					    mBloomSettings.mWeights.x = weight;

					    ImGui::TreePop();
				    }

				    if (ImGui::TreeNode("Medium##bloom"))
				    {
				    	// Iterations
				    	s32 medIter = (s32)mBloomSettings.mIterations.y;
					    ImGui::SliderInt("Iterations##medium", &medIter, 0, 30);
					    mBloomSettings.mIterations.y = medIter;

					    // Radius
					    f32 radius = (f32)mBloomSettings.mRadius.y;
					    ImGui::SliderFloat("Radius##bloomedium", &radius, 0.001f, 0.1f, "%.3f");
					    mBloomSettings.mRadius.y = radius;

					    // Weight
					    f32 weight = (f32)mBloomSettings.mWeights.y;
					    ImGui::SliderFloat("Weight##bloomedium", &weight, 0.001, 0.5f, "%.3f");
					    mBloomSettings.mWeights.y = weight;

					    ImGui::TreePop();
				    }

				    if (ImGui::TreeNode("Large##bloom"))
				    {
				    	// Iterations
				    	s32 largeIter = (s32)mBloomSettings.mIterations.z;
					    ImGui::SliderInt("Iterations##large", &largeIter, 0, 30);
					    mBloomSettings.mIterations.z = largeIter;

					    // Radius
					    f32 radius = (f32)mBloomSettings.mRadius.z;
					    ImGui::SliderFloat("Radius##bloomlarge", &radius, 0.001f, 0.1f, "%.3f");
					    mBloomSettings.mRadius.z = radius;

					    // Weight
					    f32 weight = (f32)mBloomSettings.mWeights.z;
					    ImGui::SliderFloat("Weight##bloomlarge", &weight, 0.001, 0.5f, "%.3f");
					    mBloomSettings.mWeights.z = weight;

					    ImGui::TreePop();
				    }

				    ImGui::TreePop();
		    	}

			    ImGui::SliderFloat("Scale##bloom", &mToneMapSettings.mBloomScalar, 0.01f, 100.0f, "%.2f");
			    ImGui::SliderFloat("Threshold##bloom", &mToneMapSettings.mThreshold, 0.00f, 100.0f, "%.2f");

			    ImGui::TreePop();
		    }

		    if (ImGui::TreeNode("Saturation"))
		    {
			    // Saturation
			    ImGui::SliderFloat("Saturation##tonemap", &mToneMapSettings.mSaturation, 0.0, 2.0f, "%.1f");

			    ImGui::TreePop();
		    }

		    if (ImGui::TreeNode("Gamma"))
		    {
			    // Gamma
			    ImGui::SliderFloat("Gamma##tonemap", &mToneMapSettings.mGamma, 0.01, 2.5f, "%.2f");

			    ImGui::TreePop();
		    }

		    if (ImGui::TreeNode("Exposure"))
		    {
			    // Exposure
			    ImGui::SliderFloat("Exposure##tonemap", &mToneMapSettings.mExposure, 0.01, 1.0f, "%.2f");

			    ImGui::TreePop();
		    }

		    ImGui::TreePop();
	    }

	    if (ImGui::TreeNode("FXAA"))
	    {
		    ImGui::SliderFloat("SpanMax##fxaa", &mFXAASettings.mSpanMax, 0.001, 100.0f, "%.3f");
		    ImGui::SliderFloat("ReduceMul##fxaa", &mFXAASettings.mReduceMul, 0.000001, 0.001f, "%.6f");
		    ImGui::SliderFloat("ReduceMin##fxaa", &mFXAASettings.mReduceMin, 0.000001, 0.001f, "%.6f");

		    bool enabled = (bool)mFXAASettings.mEnabled;
            ImGui::Checkbox("Enabled##fxaa", &enabled);
            mFXAASettings.mEnabled = (u32)enabled;

		    ImGui::TreePop();
	    }

	    if (ImGui::TreeNode("FrameBuffers"))
	    {
	    	ImFontAtlas* atlas = ImGui::GetIO().Fonts;
	    	ImGui::PushFont(atlas->Fonts[1]);
	        ImGui::PushStyleColor(ImGuiCol_Text, ImColor(0.2, 0.6f, 0.6f, 1.0f));
	    	for (u32 i = 0; i < (u32)GBufferTextureType::GBUFFER_TEXTURE_COUNT; ++i)
	    	{
	    		const char* string_name = mGbuffer->FrameBufferToString(i);
	    		ImGui::Text(string_name);
			    ImTextureID img = (ImTextureID)mGbuffer->GetTexture(i);

                if (ImGui::ImageButton(img, ImVec2(64, 64), ImVec2(0,1), ImVec2(1, 0), 1, ImVec4(0,0,0,0), ImColor(255,255,255,255)))
                {
			        mCurrentRenderTexture = mGbuffer->GetTexture(i); 
                }
	    	}

	    	{
	    		const char* string_name = "SmallBloom";
	    		ImGui::Text(string_name);
			    ImTextureID img = (ImTextureID)mSmallBlurVertical->GetTexture();
	            if (ImGui::ImageButton(img, ImVec2(64, 64), ImVec2(0,1), ImVec2(1, 0), 1, ImVec4(0,0,0,0), ImColor(255,255,255,255)))
	            {
			        mCurrentRenderTexture = mSmallBlurVertical->GetTexture(); 
	            }
	    	}

	    	{
	    		const char* string_name = "MediumBloom";
	    		ImGui::Text(string_name);
			    ImTextureID img = (ImTextureID)mMediumBlurVertical->GetTexture();
	            if (ImGui::ImageButton(img, ImVec2(64, 64), ImVec2(0,1), ImVec2(1, 0), 1, ImVec4(0,0,0,0), ImColor(255,255,255,255)))
	            {
			        mCurrentRenderTexture = mMediumBlurVertical->GetTexture(); 
	            }
	    	}
	    	{
	    		const char* string_name = "LargeBloom";
	    		ImGui::Text(string_name);
			    ImTextureID img = (ImTextureID)mLargeBlurVertical->GetTexture();
	            if (ImGui::ImageButton(img, ImVec2(64, 64), ImVec2(0,1), ImVec2(1, 0), 1, ImVec4(0,0,0,0), ImColor(255,255,255,255)))
	            {
			        mCurrentRenderTexture = mLargeBlurVertical->GetTexture(); 
	            }
	    	}

	    	{
	    		const char* string_name = "Final";
	    		ImGui::Text(string_name);
			    ImTextureID img = (ImTextureID)mFXAATarget->GetTexture();
	            if (ImGui::ImageButton(img, ImVec2(64, 64), ImVec2(0,1), ImVec2(1, 0), 1, ImVec4(0,0,0,0), ImColor(255,255,255,255)))
	            {
			        mCurrentRenderTexture = mFXAATarget->GetTexture(); 
	            }
	    	}

	    	ImGui::PopStyleColor(1);
	    	ImGui::PopFont();
	    	ImGui::TreePop();
	    }

	    if (ImGui::TreeNode("Sunlight"))
	    {
	    	static const char* labels[] = {"X", "Y", "Z"};
	    	Vec3 direction = mScene.GetSun()->GetDirection();
            float vec4f[4] = { direction.x, direction.y, direction.z, 1.0f };
	    	ImGui::Text("Direction");
            ImGui::DragFloat3Labels("##sundir", labels, vec4f, 0.001f, -1.0f, 1.0f);
	    	mScene.GetSun()->SetDirection(Vec3(vec4f[0], vec4f[1], vec4f[2]));

	    	float intensity = mScene.GetSun()->GetIntensity();
	    	ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 50.0f);
	    	mScene.GetSun()->SetIntensity(intensity);

	    	ImGui::TreePop();
	    }

	    if (ImGui::TreeNode("Camera"))
	    {
	    	ImGui::DragFloat("FOV", &mSceneCamera.FOV, 0.1f, 0.0f, 100.0f);
	    	ImGui::TreePop();
	    }

	    if (ImGui::TreeNode("Background"))
	    {
	    	static const char* labels[] = {"R", "G", "B"};
	    	ImGui::Text("Color");
            ImGui::DragFloat3Labels("##bgcolor", labels, mBGColor, 0.1f, 0.0f, 30.0f);
	    	ImGui::TreePop();
	    } 
	}

	//=======================================================================================================

	void DeferredRenderer::ShowGameViewport(bool* open)
	{
	    // Render game in window
		ImVec2 cursorPos = ImGui::GetCursorScreenPos( );

	    ImTextureID img = (ImTextureID)mCurrentRenderTexture;
	    ImGui::Image(img, ImVec2(ImGui::GetWindowWidth(), ImGui::GetWindowHeight()), 
	    				ImVec2(0,1), ImVec2(1,0), ImColor(255,255,255,255), ImColor(255,255,255,0));
		
		ImVec2 min = ImVec2 ( cursorPos.x + ImGui::GetContentRegionAvailWidth() - 100.0f, cursorPos.y + 10.0f );
		ImVec2 max = ImVec2( min.x + 50.0f, min.y + 10.0f );

		ImGui::SetCursorScreenPos( min );
		auto drawlist = ImGui::GetWindowDrawList( ); 
		//drawlist->AddRect( min, max, ImColor( 255, 255, 255, 255 ) );
		f32 fps = ImGui::GetIO( ).Framerate;
		drawlist->AddText( min, ImColor( 255, 255, 255, 255 ), fmt::sprintf( "Frame: %.3f", fps ).c_str() );
	} 

	void DeferredRenderer::ShaderTest( )
	{ 
		Enjon::ShaderTexture2DNode* uAlbedoMap = mShaderGraph.AddNode( new Enjon::ShaderTexture2DNode( "uAlbedoMap" ) )->Cast< Enjon::ShaderTexture2DNode >( );
		Enjon::ShaderTexture2DNode* uNormalMap = mShaderGraph.AddNode( new Enjon::ShaderTexture2DNode( "uNormalMap" ) )->Cast< Enjon::ShaderTexture2DNode >( );
		Enjon::ShaderTexture2DNode* uMetallicMap = mShaderGraph.AddNode( new Enjon::ShaderTexture2DNode( "uMetallicMap" ) )->Cast< Enjon::ShaderTexture2DNode >( );
		Enjon::ShaderTexture2DNode* uRoughnessMap = mShaderGraph.AddNode( new Enjon::ShaderTexture2DNode( "uRoughnessMap" ) )->Cast< Enjon::ShaderTexture2DNode >( );
		Enjon::ShaderTexture2DNode* uEmissiveMap = mShaderGraph.AddNode( new Enjon::ShaderTexture2DNode( "uEmissiveMap" ) )->Cast< Enjon::ShaderTexture2DNode >( );
		Enjon::ShaderFloatNode* emissiveIntensity = mShaderGraph.AddNode( new Enjon::ShaderFloatNode( "emissiveIntensity", 100.0f ) )->Cast< Enjon::ShaderFloatNode >( );
		Enjon::ShaderMultiplyNode* mult1 = mShaderGraph.AddNode( new Enjon::ShaderMultiplyNode( "mult1" ) )->Cast< Enjon::ShaderMultiplyNode >( );
		Enjon::ShaderMultiplyNode* mult2 = mShaderGraph.AddNode( new Enjon::ShaderMultiplyNode( "mult2" ) )->Cast< Enjon::ShaderMultiplyNode >( );
		Enjon::ShaderVec3Node* colRed = mShaderGraph.AddNode( new Enjon::ShaderVec3Node( "colRed", Vec3( 1.0f, 0.0f, 0.0f ) ) )->Cast< Enjon::ShaderVec3Node >( );
		Enjon::ShaderTimeNode* worldTime = mShaderGraph.AddNode( new Enjon::ShaderTimeNode( "worldTime" ) )->Cast< Enjon::ShaderTimeNode >( );
		Enjon::ShaderSinNode* sinNode = mShaderGraph.AddNode( new Enjon::ShaderSinNode( "sinNode" ) )->Cast< Enjon::ShaderSinNode >( );

		// Set up inputs to nodes 
		mult1->AddInput( ShaderGraphNode::Connection( uEmissiveMap, 0, (u32)Enjon::ShaderTexture2DNode::TexturePortType::RGB ) );
		mult1->AddInput( emissiveIntensity ); 

		sinNode->AddInput( worldTime );

		mult2->AddInput( mult1 );
		mult2->AddInput( sinNode );

		// Add inputs to main node
		mShaderGraph.Connect( Enjon::ShaderGraphNode::Connection( uAlbedoMap, ( u32 )Enjon::ShaderGraphMainNodeInputType::Albedo ) );
		mShaderGraph.Connect( Enjon::ShaderGraphNode::Connection( uMetallicMap, ( u32 )Enjon::ShaderGraphMainNodeInputType::Metallic ) );
		mShaderGraph.Connect( Enjon::ShaderGraphNode::Connection( uRoughnessMap, ( u32 )Enjon::ShaderGraphMainNodeInputType::Roughness ) );
		mShaderGraph.Connect( Enjon::ShaderGraphNode::Connection( mult2, ( u32 )Enjon::ShaderGraphMainNodeInputType::Emissive ) );
		mShaderGraph.Connect( Enjon::ShaderGraphNode::Connection( uNormalMap, ( u32 )Enjon::ShaderGraphMainNodeInputType::Normal ) );

		// Compile graph
		mShaderGraph.Compile( ); 

		// Make shader from graph
		mShader = Enjon::Shader( mShaderGraph ); 
	}
}


















