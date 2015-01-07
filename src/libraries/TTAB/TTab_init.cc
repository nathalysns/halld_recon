// $Id: TTAB_init.cc $

#include <JANA/JEventLoop.h>
using namespace jana;

#include "DTranslationTable_factory.h"

jerror_t TTAB_init(JEventLoop *loop)
{
  /// Create and register DTranslationTable factory
  loop->AddFactory(new DTranslationTable_factory());
  
  return NOERROR;
}
