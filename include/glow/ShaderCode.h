#pragma once

#include <string>

#include <glow/declspec.h>
#include <glow/ShaderSource.h>

namespace glow {

class GLOW_API ShaderCode : public ShaderSource
{
public:
	ShaderCode(const std::string& code);

	virtual const std::string& source();
protected:
	std::string _code;
};

} // namespace glow