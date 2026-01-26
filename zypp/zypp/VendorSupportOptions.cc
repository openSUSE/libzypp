
#include <zypp/VendorSupportOptions.h>
#include <zypp-core/base/Gettext.h>

namespace zypp
{

std::string
asUserString( VendorSupportOption opt )
{
    switch (opt)
    {
    case VendorSupportUnknown:
        return _("unknown");
        break;
    case VendorSupportUnsupported:
        return _("unsupported");
        break;
    case VendorSupportLevel1:
        return _("Level 1");
        break;
    case VendorSupportLevel2:
        return _("Level 2");
        break;
    case VendorSupportLevel3:
        return _("Level 3");
        break;
    case VendorSupportACC:
        return _("Additional Customer Contract Necessary");
    case VendorSupportSuperseded:
        return _("Discontinued and superseded by a different package");
    }
    return _("invalid");
}

std::string asUserStringDescription( VendorSupportOption opt )
{
    switch (opt)
    {
    case VendorSupportUnknown:
        return _("The level of support is unspecified");
        break;
    case VendorSupportUnsupported:
        return _("The vendor does not provide support.");
        break;
    case VendorSupportLevel1:
        return _("Problem determination, which means technical support designed to provide compatibility information, installation assistance, usage support, on-going maintenance and basic troubleshooting. Level 1 Support is not intended to correct product defect errors.");
        break;
    case VendorSupportLevel2:
        return _("Problem isolation, which means technical support designed to duplicate customer problems, isolate problem area and provide resolution for problems not resolved by Level 1 Support.");
        break;
    case VendorSupportLevel3:
        return _("Problem resolution, which means technical support designed to resolve complex problems by engaging engineering in resolution of product defects which have been identified by Level 2 Support.");
        break;
    case VendorSupportACC:
        return _("An additional customer contract is necessary for getting support.");
    case VendorSupportSuperseded:
        return _("The package was discontinued and has been superseded by a new package with a different name.");
    }
    return _("Unknown support option. Description not available");
}

}


