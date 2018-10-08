﻿/*
This file belongs to RenderGraph.
See LICENSE file in root folder.
*/
#include "RenderGraph/RenderPass.hpp"

#include "RenderGraph/Attachment.hpp"

#include <Image/Texture.hpp>
#include <Image/TextureView.hpp>
#include <RenderPass/RenderPassCreateInfo.hpp>

namespace crg
{
	RenderPass::RenderPass( std::string const & name
		, AttachmentArray const & inputs
		, AttachmentArray const & colourOutputs
		, std::optional< Attachment > const & depthStencilOutput )
		: name{ name }
		, inputs{ inputs }
		, colourOutputs{ colourOutputs }
		, depthStencilOutput{ depthStencilOutput }
	{
	}
}