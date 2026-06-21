#include <string_view>
#include "ysfx.hpp"

// Attach VM to a named gmem context. Empty name means default NSEEL gmem.
void ysfx_gmem_attach(ysfx_t* fx, std::string_view name);

// Detach from any currently attached named context.
void ysfx_gmem_detach(ysfx_t* fx);

void** get_gmem_address(ysfx_t* fx);
std::string get_gmem_identifier(ysfx_t* fx);
