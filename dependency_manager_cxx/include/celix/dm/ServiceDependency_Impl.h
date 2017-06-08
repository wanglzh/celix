/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 *  KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <vector>
#include <iostream>
#include "constants.h"

using namespace celix::dm;

BaseServiceDependency::BaseServiceDependency(bool v) : valid{v} {
    if (this->valid) {
        serviceDependency_create(&this->cServiceDep);
        //NOTE using suspend as default strategy
        serviceDependency_setStrategy(this->cServiceDep,  DM_SERVICE_DEPENDENCY_STRATEGY_SUSPEND);
    }
}

void BaseServiceDependency::setDepStrategy(DependencyUpdateStrategy strategy) {
    if (!valid) {
        return;
    }
    if (strategy == DependencyUpdateStrategy::locking) {
        serviceDependency_setStrategy(this->cServiceDependency(), DM_SERVICE_DEPENDENCY_STRATEGY_LOCKING);
    } else if (strategy == DependencyUpdateStrategy::suspend) {
        serviceDependency_setStrategy(this->cServiceDependency(), DM_SERVICE_DEPENDENCY_STRATEGY_SUSPEND);
    } else {
        std::cerr << "Unexpected dependency update strategy. Cannot convert for dm_depdendency\n";
    }
}

template<class T, typename I>
CServiceDependency<T,I>::CServiceDependency(const std::string name, bool valid) : TypedServiceDependency<T>(valid) {
    this->name = name;
    this->setupService();
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setVersionRange(const std::string serviceVersionRange) {
    this->versionRange = serviceVersionRange;
    this->setupService();
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setFilter(const std::string filter) {
    this->filter = filter;
    this->setupService();
    return *this;
}

template<class T, typename I>
void CServiceDependency<T,I>::setupService() {
    if (!this->valid) {
        return;
    }
    const char* cversion = this->versionRange.empty() ? nullptr : versionRange.c_str();
    const char* cfilter = filter.empty() ? nullptr : filter.c_str();
    serviceDependency_setService(this->cServiceDependency(), this->name.c_str(), cversion, cfilter);
};

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setAddLanguageFilter(bool addLang) {
//    if (!this->valid) {
//        *this;
//    }
    serviceDependency_setAddCLanguageFilter(this->cServiceDependency(), addLang);
    this->setupService();
    return *this;
};

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setRequired(bool req) {
    if (!this->valid) {
        return *this;
    }
    serviceDependency_setRequired(this->cServiceDependency(), req);
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setStrategy(DependencyUpdateStrategy strategy) {
    if (!this->valid) {
        return *this;
    }
    this->setDepStrategy(strategy);
    return *this;
}

//set callbacks
template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(void (T::*set)(const I* service)) {
    this->setCallbacks([this, set](const I* service, Properties&& __attribute__((unused)) properties) {
        T *cmp = this->componentInstance;
        (cmp->*set)(service);
    });
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(void (T::*set)(const I* service, Properties&& properties)) {
    this->setCallbacks([this, set](const I* service, Properties&& properties) {
        T *cmp = this->componentInstance;
        (cmp->*set)(service, std::move(properties));
    });
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(std::function<void(const I* service, Properties&& properties)> set) {
    this->setFp = set;
    this->setupCallbacks();
    return *this;
}

//add remove callbacks
template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(
        void (T::*add)(const I* service),
        void (T::*remove)(const I* service)) {
    this->setCallbacks(
		    [this, add](const I* service, Properties&& __attribute__((unused)) properties) {
			    T *cmp = this->componentInstance;
			    (cmp->*add)(service);
		    },
		    [this, remove](const I* service, Properties&& __attribute__((unused)) properties) {
			    T *cmp = this->componentInstance;
			    (cmp->*remove)(service);
		    }
    );
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(
        void (T::*add)(const I* service, Properties&& properties),
        void (T::*remove)(const I* service, Properties&& properties)
) {
    this->setCallbacks(
		    [this, add](const I* service, Properties&& properties) {
			    T *cmp = this->componentInstance;
			    (cmp->*add)(service, std::move(properties));
		    },
		    [this, remove](const I* service, Properties&& properties) {
			    T *cmp = this->componentInstance;
			    (cmp->*remove)(service, std::move(properties));
		    }
    );
    return *this;
}

template<class T, typename I>
CServiceDependency<T,I>& CServiceDependency<T,I>::setCallbacks(std::function<void(const I* service, Properties&& properties)> add, std::function<void(const I* service, Properties&& properties)> remove) {
    this->addFp = add;;
    this->removeFp = remove;
    this->setupCallbacks();
    return *this;
}


template<class T, typename I>
void CServiceDependency<T,I>::setupCallbacks() {
    if (!this->valid) {
        return;
    }

    int(*cset)(void*, service_reference_pt, const void*) {nullptr};
    int(*cadd)(void*, service_reference_pt, const void*) {nullptr};
    int(*crem)(void*, service_reference_pt, const void*) {nullptr};

    if (setFp != nullptr) {
        cset = [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (CServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->setFp, ref, service);
        };
    }
    if (addFp != nullptr) {
        cadd = [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (CServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->addFp, ref, service);
        };
    }
    if (removeFp != nullptr) {
        crem= [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (CServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->removeFp, ref, service);
        };
    }
    serviceDependency_setCallbackHandle(this->cServiceDependency(), this);
    serviceDependency_setCallbacksWithServiceReference(this->cServiceDependency(), cset, cadd, nullptr, crem, nullptr);
}

template<class T, typename I>
int CServiceDependency<T,I>::invokeCallback(std::function<void(const I*, Properties&&)> fp, service_reference_pt  ref, const void* service) {
    service_registration_pt reg {nullptr};
    properties_pt props {nullptr};

    Properties properties {};
    const char* key {nullptr};
    const char* value {nullptr};

    if (ref != nullptr) {
        serviceReference_getServiceRegistration(ref, &reg);
        serviceRegistration_getProperties(reg, &props);
        
        hash_map_iterator_t iter = hashMapIterator_construct((hash_map_pt)props);
        while(hashMapIterator_hasNext(&iter)) {
            key = (const char*) hashMapIterator_nextKey(&iter);
            value = properties_get(props, key);
            //std::cout << "got property " << key << "=" << value << "\n";
            properties[key] = value;
        }
    }

    const I* srv = (const I*) service;

    fp(srv, std::move(properties));
    return 0;
}

template<class T, class I>
ServiceDependency<T,I>::ServiceDependency(std::string name, bool valid) : TypedServiceDependency<T>(valid) {
    if (!name.empty()) {
        this->setName(name);
    } else {
        this->setupService();
    }
};

template<class T, class I>
void ServiceDependency<T,I>::setupService() {
    if (!this->valid) {
        return;
    }

    std::string n = name;

    if (n.empty()) {
        n = typeName<I>();
        if (n.empty()) {
            std::cerr << "Error in service dependency. Type name cannot be inferred, using '<TYPE_NAME_ERROR>'. function: '" << __PRETTY_FUNCTION__ << "'\n";
            n = "<TYPE_NAME_ERROR>";
        }
    }

    const char* v =  versionRange.empty() ? nullptr : versionRange.c_str();


    if (this->addCxxLanguageFilter) {

        char langFilter[128];
        snprintf(langFilter, sizeof(langFilter), "(%s=%s)", CELIX_FRAMEWORK_SERVICE_LANGUAGE,
                 CELIX_FRAMEWORK_SERVICE_CXX_LANGUAGE);

        //setting modified filter. which is in a filter including a lang=c++
        modifiedFilter = std::string{langFilter};
        if (!filter.empty()) {
            char needle[128];
            snprintf(needle, sizeof(needle), "(%s=", CELIX_FRAMEWORK_SERVICE_LANGUAGE);
            size_t langFilterPos = filter.find(needle);
            if (langFilterPos != std::string::npos) {
                //do nothing filter already contains a language part.
            } else if (strncmp(filter.c_str(), "(&", 2) == 0 && filter[filter.size() - 1] == ')') {
                modifiedFilter = filter.substr(0, filter.size() - 1); //remove closing ')'
                modifiedFilter = modifiedFilter.append(langFilter);
                modifiedFilter = modifiedFilter.append(")"); //add closing ')'
            } else if (filter[0] == '(' && filter[filter.size() - 1] == ')') {
                modifiedFilter = "(&";
                modifiedFilter = modifiedFilter.append(filter);
                modifiedFilter = modifiedFilter.append(langFilter);
                modifiedFilter = modifiedFilter.append(")");
            } else {
                std::cerr << "Unexpected filter layout: '" << filter << "'\n";
            }
        }
    } else {
        this->modifiedFilter = this->filter;
    }

    serviceDependency_setService(this->cServiceDependency(), n.c_str(), v, this->modifiedFilter.c_str());
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setName(std::string name) {
    this->name = name;
    setupService();
    return *this;
};

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setFilter(std::string filter) {
    this->filter = filter;
    setupService();
    return *this;
};

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setVersionRange(std::string versionRange) {
    this->versionRange = versionRange;
    setupService();
    return *this;
};


template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setAddLanguageFilter(bool addLang) {
    this->addCxxLanguageFilter = addLang;
    setupService();
    return *this;
};

//set callbacks
template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(void (T::*set)(I* service)) {
    this->setCallbacks([this, set](I* srv, Properties&& __attribute__((unused)) props) {
        T *cmp = this->componentInstance;
        (cmp->*set)(srv);
    });
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(void (T::*set)(I* service, Properties&& properties)) {
    this->setCallbacks([this, set](I* srv, Properties&& props) {
        T *cmp = this->componentInstance;
        (cmp->*set)(srv, std::move(props));
    });
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(std::function<void(I* service, Properties&& properties)> set) {
    this->setFp = set;
    this->setupCallbacks();
    return *this;
}

//add remove callbacks
template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(
        void (T::*add)(I* service),
        void (T::*remove)(I* service)) {
    this->setCallbacks(
	    [this, add](I* srv, Properties&& __attribute__((unused)) props) {
        	T *cmp = this->componentInstance;
        	(cmp->*add)(srv);
    	    },
	    [this, remove](I* srv, Properties&& __attribute__((unused)) props) {
        	T *cmp = this->componentInstance;
        	(cmp->*remove)(srv);
    	    }
    );
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(
        void (T::*add)(I* service, Properties&& properties),
        void (T::*remove)(I* service, Properties&& properties)
        ) {
    this->setCallbacks(
	    [this, add](I* srv, Properties&& props) {
        	T *cmp = this->componentInstance;
        	(cmp->*add)(srv, std::move(props));
    	    },
	    [this, remove](I* srv, Properties&& props) {
        	T *cmp = this->componentInstance;
        	(cmp->*remove)(srv, std::move(props));
    	    }
    );
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setCallbacks(
		std::function<void(I* service, Properties&& properties)> add,
		std::function<void(I* service, Properties&& properties)> remove) {
    this->addFp = add;
    this->removeFp = remove;
    this->setupCallbacks();
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setRequired(bool req) {
    serviceDependency_setRequired(this->cServiceDependency(), req);
    return *this;
}

template<class T, class I>
ServiceDependency<T,I>& ServiceDependency<T,I>::setStrategy(DependencyUpdateStrategy strategy) {
    this->setDepStrategy(strategy);
    return *this;
};

template<class T, class I>
int ServiceDependency<T,I>::invokeCallback(std::function<void(I*, Properties&&)> fp, service_reference_pt  ref, const void* service) {
    service_registration_pt reg {nullptr};
    properties_pt props {nullptr};
    I *svc = (I*)service;

    Properties properties {};
    const char* key {nullptr};
    const char* value {nullptr};

    if (ref != nullptr) {
        serviceReference_getServiceRegistration(ref, &reg);
        serviceRegistration_getProperties(reg, &props);
        hash_map_iterator_t iter = hashMapIterator_construct((hash_map_pt)props);
        while(hashMapIterator_hasNext(&iter)) {
            key = (const char*) hashMapIterator_nextKey(&iter);
            value = properties_get(props, key);
            //std::cout << "got property " << key << "=" << value << "\n";
            properties[key] = value;
        }
    }

    fp(svc, std::move(properties)); //explicit move of lvalue properties.
    return 0;
}

template<class T, class I>
void ServiceDependency<T,I>::setupCallbacks() {
    if (!this->valid) {
        return;
    }

    int(*cset)(void*, service_reference_pt, const void*) {nullptr};
    int(*cadd)(void*, service_reference_pt, const void*) {nullptr};
    int(*crem)(void*, service_reference_pt, const void*) {nullptr};

    if (setFp != nullptr) {
        cset = [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (ServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->setFp, ref, service);
        };
    }
    if (addFp != nullptr) {
        cadd = [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (ServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->addFp, ref, service);
        };
    }
    if (removeFp != nullptr) {
        crem = [](void* handle, service_reference_pt ref, const void* service) -> int {
            auto dep = (ServiceDependency<T,I>*) handle;
            return dep->invokeCallback(dep->removeFp, ref, service);
        };
    }

    serviceDependency_setCallbackHandle(this->cServiceDependency(), this);
    serviceDependency_setCallbacksWithServiceReference(this->cServiceDependency(), cset, cadd, nullptr, crem, nullptr);
};
