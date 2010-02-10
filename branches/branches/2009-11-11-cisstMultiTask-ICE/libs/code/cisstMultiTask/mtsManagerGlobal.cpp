/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-    */
/* ex: set filetype=cpp softtabstop=4 shiftwidth=4 tabstop=4 cindent expandtab: */

/*
  $Id: mtsManagerGlobal.h 794 2009-09-01 21:43:56Z pkazanz1 $

  Author(s):  Min Yang Jung
  Created on: 2009-11-12

  (C) Copyright 2009-2010 Johns Hopkins University (JHU), All Rights
  Reserved.

--- begin cisst license - do not edit ---

This software is provided "as is" under an open source license, with
no warranty.  The complete license can be found in license.txt and
http://www.cisst.org/cisst/license.txt.

--- end cisst license ---
*/

#include <cisstMultiTask/mtsConfig.h>
#include <cisstOSAbstraction/osaSocket.h>
#include <cisstMultiTask/mtsManagerGlobal.h>

#if CISST_MTS_HAS_ICE
#include <cisstMultiTask/mtsManagerProxyServer.h>
#endif // CISST_MTS_HAS_ICE

CMN_IMPLEMENT_SERVICES(mtsManagerGlobal);

mtsManagerGlobal::mtsManagerGlobal() : LocalManagerConnected(0)
{
    ConnectionID = static_cast<unsigned int>(mtsManagerGlobalInterface::CONNECT_ID_BASE);
}

mtsManagerGlobal::~mtsManagerGlobal()
{
    CleanUp();
}

//-------------------------------------------------------------------------
//  Processing Methods
//-------------------------------------------------------------------------
bool mtsManagerGlobal::CleanUp(void)
{    
    bool ret = true;

    // Remove all processes safely
    ProcessMapType::iterator itProcess = ProcessMap.begin();
    while (itProcess != ProcessMap.end()) {        
        ret &= RemoveProcess(itProcess->first);
        itProcess = ProcessMap.begin();
    }

    // Remove all connection elements
    ConnectionElementMapType::const_iterator itConnection = ConnectionElementMap.begin();
    const ConnectionElementMapType::const_iterator itConnectionEnd = ConnectionElementMap.begin();
    for (; itConnection != itConnectionEnd; ++itConnection) {
        delete itConnection->second;
    }

    return ret;
}

//-------------------------------------------------------------------------
//  Process Management
//-------------------------------------------------------------------------
bool mtsManagerGlobal::AddProcess(const std::string & processName)
{
    // Check if the local component manager has already been registered.
    if (FindProcess(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProcess: already registered process: " << processName << std::endl;
        return false;
    }
    
    // Register to process map
    if (!ProcessMap.AddItem(processName, NULL)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProcess: failed to add process to process map: " << processName << std::endl;
        return false;
    }

    return true;
}

bool mtsManagerGlobal::AddProcessObject(mtsManagerLocalInterface * localManagerObject, const bool isManagerProxyServer)
{
    if (LocalManagerConnected) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProcessObject: local manager object has already been registered." << std::endl;
        return false;
    }

    // Name of local component manager which is now connecting
    std::string processName;

    // If localManagerObject is of type mtsManagerProxyServer, process name can
    // be set without calling mtsManagerLocalInterface::GetProcessName()
    if (isManagerProxyServer) {
#if CISST_MTS_HAS_ICE
        mtsManagerProxyServer * managerProxyServer = dynamic_cast<mtsManagerProxyServer *>(localManagerObject);
        if (!managerProxyServer) {
            CMN_LOG_CLASS_RUN_ERROR << "AddProcessObject: invalid object type (mtsManagerProxyServer expected)" << std::endl;
            return false;
        }
        processName = managerProxyServer->GetProxyName();
#endif
    } else {
        processName = localManagerObject->GetProcessName();
    }

    // Check if the local component manager has already been registered.
    if (FindProcess(processName)) {
        // Update LocalManagerMap
        LocalManagerConnected = localManagerObject;

        CMN_LOG_CLASS_RUN_VERBOSE << "AddProcessObject: updated local manager object" << std::endl;
        return true;
    } 
    
    // Register to process map
    if (!ProcessMap.AddItem(processName, NULL)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProcessObject: failed to add process to process map: " << processName << std::endl;
        return false;
    }

    // Register to local manager object map
    LocalManagerConnected = localManagerObject;

    return true;
}

bool mtsManagerGlobal::FindProcess(const std::string & processName) const
{
    return ProcessMap.FindItem(processName);
}

mtsManagerLocalInterface * mtsManagerGlobal::GetProcessObject(const std::string & processName)
{
    if (!ProcessMap.FindItem(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "GetProcessObject: Can't find registered process: " << processName << std::endl;
        return false;
    }

    return LocalManagerConnected;
}

bool mtsManagerGlobal::RemoveProcess(const std::string & processName)
{
    if (!FindProcess(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "RemoveProcess: Can't find registered process: " << processName << std::endl;
        return false;
    }

    bool ret = true;
    ComponentMapType * componentMap = ProcessMap.GetItem(processName);

    // When componentMap is not NULL, all components that the process manages 
    // should be removed first.
    if (componentMap) {
        ComponentMapType::iterator it = componentMap->begin();
        while (it != componentMap->end()) {
            ret &= RemoveComponent(processName, it->first);
            it = componentMap->begin();
        }
        
        delete componentMap;
    }

    // Remove the process from process map
    ret &= ProcessMap.RemoveItem(processName);

    return ret;
}

//-------------------------------------------------------------------------
//  Component Management
//-------------------------------------------------------------------------
bool mtsManagerGlobal::AddComponent(const std::string & processName, const std::string & componentName)
{
    if (!FindProcess(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddComponent: Can't find registered process: " << processName << std::endl;
        return false;
    }

    ComponentMapType * componentMap = ProcessMap.GetItem(processName);

    // If the process did not register before
    if (componentMap == NULL) {
        componentMap = new ComponentMapType(processName);
        (ProcessMap.GetMap())[processName] = componentMap;
    }

    bool ret = componentMap->AddItem(componentName, NULL);
    if (!ret) {
        CMN_LOG_CLASS_RUN_ERROR << "AddComponent: Can't add a component: " 
            << "\"" << processName << "\" - \"" << componentName << "\"" << std::endl;
    }

    return ret;
}

bool mtsManagerGlobal::FindComponent(const std::string & processName, const std::string & componentName) const
{
    if (!FindProcess(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "FindComponent: Can't find a registered process: " << processName << std::endl;
        return false;
    }

    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    if (!componentMap) {
        return false;
    }

    return componentMap->FindItem(componentName);
}

bool mtsManagerGlobal::RemoveComponent(const std::string & processName, const std::string & componentName)
{
    // Check if the component has been registered
    if (!FindComponent(processName, componentName)) {
        CMN_LOG_CLASS_RUN_ERROR << "RemoveComponent: Can't find component: " 
            << processName << ":" << componentName << std::endl;
        return false;
    }

    bool ret = true;

    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    CMN_ASSERT(componentMap);

    // When interfaceMapType is not NULL, all interfaces that the component manages
    // should be removed first.
    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);
    if (interfaceMap) {
        ConnectedInterfaceMapType::iterator it;

        // Remove all the required interfaces that the process manage.
        it = interfaceMap->RequiredInterfaceMap.GetMap().begin();
        while (it != interfaceMap->RequiredInterfaceMap.GetMap().end()) {
            ret &= RemoveRequiredInterface(processName, componentName, it->first);
            it = interfaceMap->RequiredInterfaceMap.GetMap().begin();
        }

        // Remove all the provided interfaces that the process manage.
        it = interfaceMap->ProvidedInterfaceMap.GetMap().begin();
        while (it != interfaceMap->ProvidedInterfaceMap.GetMap().end()) {
            ret &= RemoveProvidedInterface(processName, componentName, it->first);
            it = interfaceMap->ProvidedInterfaceMap.GetMap().begin();
        }

        delete interfaceMap;
    }

    // Remove the component from component map
    ret &= componentMap->RemoveItem(componentName);

    return ret;
}

//-------------------------------------------------------------------------
//  Interface Management
//-------------------------------------------------------------------------
bool mtsManagerGlobal::AddProvidedInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName, const bool isProxyInterface)
{
    if (!FindComponent(processName, componentName)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProvidedInterface: Can't find a registered component: " 
            << "\"" << processName << "\" - \"" << componentName << "\"" << std::endl;
        return false;
    }

    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);

    // If the component did not have any interface before
    if (interfaceMap == NULL) {
        interfaceMap = new InterfaceMapType;
        (componentMap->GetMap())[componentName] = interfaceMap;
    }

    // Add the interface    
    if (!interfaceMap->ProvidedInterfaceMap.AddItem(interfaceName, NULL)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddProvidedInterface: Can't add a provided interface: " << interfaceName << std::endl;
        return false;
    }

    interfaceMap->ProvidedInterfaceTypeMap[interfaceName] = isProxyInterface;

    return true;
}

bool mtsManagerGlobal::AddRequiredInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName, const bool isProxyInterface)
{
    if (!FindComponent(processName, componentName)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddRequiredInterface: Can't find a registered component: " 
            << "\"" << processName << "\" - \"" << componentName << "\"" << std::endl;
        return false;
    }

    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);

    // If the component did not registered before
    if (interfaceMap == NULL) {
        interfaceMap = new InterfaceMapType;
        (componentMap->GetMap())[componentName] = interfaceMap;
    }

    // Add the interface    
    if (!interfaceMap->RequiredInterfaceMap.AddItem(interfaceName, NULL)) {
        CMN_LOG_CLASS_RUN_ERROR << "AddRequiredInterface: Can't add a required interface: " << interfaceName << std::endl;
        return false;
    }

    interfaceMap->RequiredInterfaceTypeMap[interfaceName] = isProxyInterface;

    return true;
}

bool mtsManagerGlobal::FindProvidedInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName) const
{
    if (!FindComponent(processName, componentName)) {
        CMN_LOG_CLASS_RUN_ERROR << "FindProvidedInterface: Can't find a registered component: " 
            << "\"" << processName << "\" - \"" << componentName << "\"" << std::endl;
        return false;
    }
    
    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    if (!componentMap) return false;

    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);
    if (!interfaceMap) return false;

    return interfaceMap->ProvidedInterfaceMap.FindItem(interfaceName);
}

bool mtsManagerGlobal::FindRequiredInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName) const
{
    if (!FindComponent(processName, componentName)) {
        CMN_LOG_CLASS_RUN_ERROR << "FindRequiredInterface: Can't find a registered component: " 
            << "\"" << processName << "\" - \"" << componentName << "\"" << std::endl;
        return false;
    }
    
    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    if (!componentMap) return false;

    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);
    if (!interfaceMap) return false;

    return interfaceMap->RequiredInterfaceMap.FindItem(interfaceName);
}

bool mtsManagerGlobal::RemoveProvidedInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName)
{
    // Check existence of the process        
    if (!ProcessMap.FindItem(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "RemoveProvidedInterface: Can't find registered process: " << processName << std::endl;
        return false;
    }

    // Check existence of the component
    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    if (!componentMap) return false;
    if (!componentMap->FindItem(componentName)) return false;

    // Check existence of the provided interface
    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);
    if (!interfaceMap) return false;
    if (!interfaceMap->ProvidedInterfaceMap.FindItem(interfaceName)) return false;

    // When connectionMap is not NULL, all connection information that the provided interface
    // has should be removed first.
    bool ret = true;
    ConnectionMapType * connectionMap = interfaceMap->ProvidedInterfaceMap.GetItem(interfaceName);
    if (connectionMap) {
        ConnectionMapType::iterator it = connectionMap->begin();
        while (it != connectionMap->end()) {
            Disconnect(it->second->GetProcessName(), it->second->GetComponentName(), it->second->GetInterfaceName(),
                processName, componentName, interfaceName);
            it = connectionMap->begin();
        }
        delete connectionMap;
    }

    // Remove the provided interface from provided interface map
    ret &= interfaceMap->ProvidedInterfaceMap.RemoveItem(interfaceName);

    return ret;
}

bool mtsManagerGlobal::RemoveRequiredInterface(
    const std::string & processName, const std::string & componentName, const std::string & interfaceName)
{
    // Check if the process exists
    if (!ProcessMap.FindItem(processName)) {
        CMN_LOG_CLASS_RUN_ERROR << "RemoveRequiredInterface: Can't find registered process: " << processName << std::endl;
        return false;
    }

    // Check if the component exists
    ComponentMapType * componentMap = ProcessMap.GetItem(processName);
    CMN_ASSERT(componentMap);
    if (!componentMap->FindItem(componentName)) return false;

    // Check if the provided interface exists
    InterfaceMapType * interfaceMap = componentMap->GetItem(componentName);
    if (!interfaceMap) return false;
    if (!interfaceMap->RequiredInterfaceMap.FindItem(interfaceName)) return false;

    // When connectionMap is not NULL, all the connections that the provided 
    // interface is connected to should be removed.
    bool ret = true;
    ConnectionMapType * connectionMap = interfaceMap->RequiredInterfaceMap.GetItem(interfaceName);
    if (connectionMap) {
        ConnectionMapType::iterator it = connectionMap->begin();
        while (it != connectionMap->end()) {
            Disconnect(processName, componentName, interfaceName, 
                it->second->GetProcessName(), it->second->GetComponentName(), it->second->GetInterfaceName());
            it = connectionMap->begin();
        }
        delete connectionMap;
    }

    // Remove the required interface from provided interface map
    ret &= interfaceMap->RequiredInterfaceMap.RemoveItem(interfaceName);

    return ret;
}

//-------------------------------------------------------------------------
//  Connection Management
//-------------------------------------------------------------------------
unsigned int mtsManagerGlobal::Connect(const std::string & requestProcessName,
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName)
{
    const unsigned int retError = static_cast<unsigned int>(mtsManagerGlobalInterface::CONNECT_ERROR);

    // Check if the required interface specified actually exist.
    if (!FindRequiredInterface(clientProcessName, clientComponentName, clientRequiredInterfaceName)) {
        CMN_LOG_CLASS_RUN_VERBOSE << "Connect: required interface does not exist: "
            << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName)
            << std::endl;
        return retError;
    }

    // Check if the provided interface specified actually exist.
    if (!FindProvidedInterface(serverProcessName, serverComponentName, serverProvidedInterfaceName)) {
        CMN_LOG_CLASS_RUN_VERBOSE << "Connect: provided interface does not exist: "
            << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName)
            << std::endl;
        return retError;
    }

    // Check if the two interfaces are already connected.
    int ret = IsAlreadyConnected(clientProcessName,
                                 clientComponentName,
                                 clientRequiredInterfaceName,
                                 serverProcessName,
                                 serverComponentName,
                                 serverProvidedInterfaceName);
    // When error occurs (because of non-existing components, etc)
    if (ret < 0) {
        CMN_LOG_CLASS_RUN_VERBOSE << "Connect: "
            << "one or more processes, components, or interfaces are missing." << std::endl;
        return retError;
    }
    // When interfaces have already been connected to each other
    else if (ret > 0) {
        CMN_LOG_CLASS_RUN_VERBOSE << "Connect: Two interfaces are already connected: "
            << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName)
            << " and "
            << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName)
            << std::endl;
        return retError;
    }

    // Assign a new connection ID which is increased only if this method returns true.
    unsigned int thisConnectionID = ConnectionID;

    // In case of remote connection
#if CISST_MTS_HAS_ICE
    if (clientProcessName != serverProcessName) {
        // Term definitions
        // - Server manager: local component manager that manages server components
        // - Client manager: local component manager that manages client components

        // Check if the server manager has client component proxies.
        // If not, create one.
        const std::string clientComponentProxyName = GetComponentProxyName(clientProcessName, clientComponentName);
        if (!FindComponent(serverProcessName, clientComponentProxyName)) {
            if (!LocalManagerConnected->CreateComponentProxy(clientComponentProxyName, serverProcessName)) {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to create client component proxy" << std::endl;
                return false;
            }
            CMN_LOG_CLASS_RUN_VERBOSE << "ProcessConnectionQueue: client component proxy is created: " 
                << clientComponentProxyName << " on " << serverProcessName << std::endl;
        }

        // Check if the client manager has the client component proxy. If not,
        // create one.
        const std::string serverComponentProxyName = GetComponentProxyName(serverProcessName, serverComponentName);
        if (!FindComponent(clientProcessName, serverComponentProxyName)) {
            if (!LocalManagerConnected->CreateComponentProxy(serverComponentProxyName, clientProcessName)) {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to create server component proxy" << std::endl;
                return false;
            }
            CMN_LOG_CLASS_RUN_VERBOSE << "ProcessConnectionQueue: server component proxy is created: " 
                << serverComponentProxyName << " on " << clientProcessName << std::endl;
        }

        // Check if the specified interfaces exist in each process. If not,
        // create an interface proxy.
        // Note that, under the current design, a required interface can connect
        // to multiple provided interfaces whereas a required interface connects
        // to only one provided interface.
        // Thus, a required interface proxy is created whenever a server component
        // doesn't have it while a provided interface proxy is generated only at 
        // the first time when a client component doesn't have it.
        
        // Check if provided interface proxy already exists at the client side.
        bool foundProvidedInterfaceProxy = FindProvidedInterface(clientProcessName, serverComponentProxyName, serverProvidedInterfaceName);

        // Check if required interface proxy already exists at the server side.
        bool foundRequiredInterfaceProxy = FindRequiredInterface(serverProcessName, clientComponentProxyName, clientRequiredInterfaceName);

        // Create an interface proxy (or proxies) as needed.
        //
        // From the server manager and the client manager, extract the 
        // information about the two interfaces specified. The global component
        // manager will deliver this information to a peer local component 
        // manager so that they can create proxy components.
        //
        // Note that required interface proxy has to be created first because
        // pointers to function proxy objects in the required interface should
        // be available in order to create the provided interface.

        // Create required interface proxy
        if (!foundRequiredInterfaceProxy) {
            // Extract required interface description
            RequiredInterfaceDescription requiredInterfaceDescription;
            if (!LocalManagerConnected->GetRequiredInterfaceDescription(
                clientComponentName, clientRequiredInterfaceName, requiredInterfaceDescription, clientProcessName)) 
            {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to get required interface description: "
                    << clientProcessName << ":" << clientComponentName << ":" << clientRequiredInterfaceName << std::endl;
                return false;
            }

            // Create required interface proxy at the server side
            if (!LocalManagerConnected->CreateRequiredInterfaceProxy(clientComponentProxyName, requiredInterfaceDescription, serverProcessName)) {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to create required interface proxy: "
                    << clientComponentProxyName << " in " << serverProcessName << std::endl;
                return false;
            }
        }

        // Create provided interface proxy
        if (!foundProvidedInterfaceProxy) {
            // Extract provided interface description
            ProvidedInterfaceDescription providedInterfaceDescription;
            if (!LocalManagerConnected->GetProvidedInterfaceDescription(
                serverComponentName, serverProvidedInterfaceName, providedInterfaceDescription, serverProcessName)) 
            {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to get provided interface description: "
                    << serverProcessName << ":" << serverComponentName << ":" << serverProvidedInterfaceName << std::endl;
                return false;
            }

            // Create provided interface proxy at the client side
            if (!LocalManagerConnected->CreateProvidedInterfaceProxy(serverComponentProxyName, providedInterfaceDescription, clientProcessName)) {
                CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: failed to create provided interface proxy: "
                    << serverComponentProxyName << " in " << clientProcessName << std::endl;
                return false;
            }
        }

        // TODO:
        // 4. let two LCMs create proxy components
        // 5. wait for responses from LCMs
        //    - if timeouts, call disconnect to break and clean current connection
        //    - if success at both sides, update command id and event handler id
    }
#endif

    InterfaceMapType * interfaceMap;

    // Connect client's required interface with server's provided interface.
    ConnectionMapType * connectionMap = GetConnectionsOfRequiredInterface(
        clientProcessName, clientComponentName, clientRequiredInterfaceName, &interfaceMap);
    // If the required interface has never connected to other provided interfaces
    if (connectionMap == NULL) {
        // Create a connection map for the required interface
        connectionMap = new ConnectionMapType(clientRequiredInterfaceName);
        (interfaceMap->RequiredInterfaceMap.GetMap())[clientRequiredInterfaceName] = connectionMap;
    }

    // Add an element containing information about the connected provided interface
    bool isRemoteConnection = (clientProcessName != serverProcessName);
    if (!AddConnectedInterface(connectionMap, serverProcessName, serverComponentName, serverProvidedInterfaceName, isRemoteConnection)) {
        CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: "
            << "failed to add information about connected provided interface." << std::endl;
        return false;
    }

    // Connect server's provided interface with client's required interface.
    connectionMap = GetConnectionsOfProvidedInterface(serverProcessName,
                                                      serverComponentName,
                                                      serverProvidedInterfaceName,
                                                      &interfaceMap);
    // If the provided interface has never been connected with other required interfaces
    if (connectionMap == NULL) {
        // Create a connection map for the provided interface
        connectionMap = new ConnectionMapType(serverProvidedInterfaceName);
        (interfaceMap->ProvidedInterfaceMap.GetMap())[serverProvidedInterfaceName] = connectionMap;
    }

    // Add an element containing information about the connected provided interface
    if (!AddConnectedInterface(connectionMap, clientProcessName, clientComponentName, clientRequiredInterfaceName, isRemoteConnection)) {
        CMN_LOG_CLASS_RUN_ERROR << "ProcessConnectionQueue: "
            << "failed to add information about connected required interface." << std::endl;

        // Before returning false, should clean up required interface's connection information
        Disconnect(clientProcessName, clientComponentName, clientRequiredInterfaceName,
                   serverProcessName, serverComponentName, serverProvidedInterfaceName);
        return false;
    }

    CMN_LOG_CLASS_RUN_VERBOSE << "ProcessConnectionQueue: successfully connected: " 
        << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName) << " - "
        << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName) << std::endl;


    // Create an instance of ConnectionElement
    ConnectionElement * element = new ConnectionElement(requestProcessName, thisConnectionID,
        clientProcessName, clientComponentName, clientRequiredInterfaceName,
        serverProcessName, serverComponentName, serverProvidedInterfaceName);

    ConnectionElementMapChange.Lock();
    ConnectionElementMap.insert(std::make_pair(ConnectionID, element));
    ConnectionElementMapChange.Unlock();

    // Increase connection counter
    ++ConnectionID;

    return thisConnectionID;
}

#if CISST_MTS_HAS_ICE
void mtsManagerGlobal::ConnectCheckTimeout()
{
    if (ConnectionElementMap.empty()) return;

    ConnectionElementMapChange.Lock();

    ConnectionElement * element;
    ConnectionElementMapType::iterator it = ConnectionElementMap.begin();
    for (; it != ConnectionElementMap.end(); ) {
        element = it->second;

        // Skip timeout check if connection has been already established.
        if (element->Connected) {
            ++it;
            continue;
        }

        // Timeout check
        if (!element->CheckTimeout()) {
            ++it;
            continue;
        }

        CMN_LOG_CLASS_RUN_VERBOSE << "ConnectCheckTimeout: Disconnect due to timeout: Connection session id: " << element->ConnectionID << std::endl;
        CMN_LOG_CLASS_RUN_VERBOSE << "ConnectCheckTimeout: Remove an element: " 
            << GetInterfaceUID(element->ClientProcessName, element->ClientComponentName, element->ClientRequiredInterfaceName) << " - "
            << GetInterfaceUID(element->ServerProcessName, element->ServerComponentName, element->ServerProvidedInterfaceName) << std::endl;

        // Remove an element
        delete element;
        ConnectionElementMap.erase(it++);
    }

    ConnectionElementMapChange.Unlock();
}
#endif

bool mtsManagerGlobal::ConnectConfirm(unsigned int connectionSessionID)
{
    ConnectionElementMapType::const_iterator it = ConnectionElementMap.find(connectionSessionID);
    if (it == ConnectionElementMap.end()) {
        CMN_LOG_CLASS_RUN_ERROR << "ConnectConfirm: invalid session id: " << connectionSessionID << std::endl;
        return false;
    }

    ConnectionElement * element = it->second;
    element->SetConnected();

    CMN_LOG_CLASS_RUN_VERBOSE << "ConnectConfirm: connection (id: " << connectionSessionID << ") is confirmed" << std::endl;

    return true;
}

bool mtsManagerGlobal::IsAlreadyConnected(
    const std::string & clientProcessName,
    const std::string & clientComponentName,
    const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName,
    const std::string & serverComponentName,
    const std::string & serverProvidedInterfaceName)
{
    // It is assumed that the existence of interfaces has already been checked before
    // calling this method.

    // Check if the required interface is connected to the provided interface
    ConnectionMapType * connectionMap = GetConnectionsOfRequiredInterface(
        clientProcessName, clientComponentName, clientRequiredInterfaceName);
    if (connectionMap) {
        if (connectionMap->FindItem(GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName)))
        {
            CMN_LOG_CLASS_RUN_VERBOSE << "Already established connection" << std::endl;
            return true;
        }
    }

    // Check if the provided interface is connected to the required interface
    connectionMap = GetConnectionsOfProvidedInterface(
        serverProcessName, serverComponentName, serverProvidedInterfaceName);
    if (connectionMap) {
        if (connectionMap->FindItem(GetInterfaceUID(clientProcessName, clientComponentName, clientProcessName)))
        {
            CMN_LOG_CLASS_RUN_VERBOSE << "Already established connection" << std::endl;
            return true;
        }
    }

    return false;
}

bool mtsManagerGlobal::Disconnect(
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName)
{
    // Get connection information
    ConnectionMapType * connectionMapOfRequiredInterface = GetConnectionsOfRequiredInterface(
        clientProcessName, clientComponentName, clientRequiredInterfaceName);
    if (!connectionMapOfRequiredInterface) {
        CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to disconnect. Required interface has no connection: "
            << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName) << std::endl;
        return false;
    }

    ConnectionMapType * connectionMapOfProvidedInterface = GetConnectionsOfProvidedInterface(
        serverProcessName, serverComponentName, serverProvidedInterfaceName);
    if (!connectionMapOfProvidedInterface) {
        CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to disconnect. Provided interface has no connection: "
            << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName) << std::endl;
        return false;
    }
    
    bool remoteConnection = false;  // true if the connection to be disconnected is a remote connection.
    std::string interfaceUID;
    ConnectedInterfaceInfo * connectionInfo;

    // Remove required interfaces' connection information
    if (connectionMapOfRequiredInterface->size()) {
        // Get an element that contains connection information
        interfaceUID = GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName);
        connectionInfo = connectionMapOfRequiredInterface->GetItem(interfaceUID);        

        // Release allocated memory
        if (connectionInfo) {
            remoteConnection = connectionInfo->IsRemoteConnection();
            delete connectionInfo;
        }

        // Remove connection information
        if (connectionMapOfRequiredInterface->FindItem(interfaceUID)) {
            if (!connectionMapOfRequiredInterface->RemoveItem(interfaceUID)) {
                CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to update connection map in server process" << std::endl;
                return false;
            }

            // If the required interface is a proxy object (not an original interface), 
            // it should be removed when the connection is disconnected.
#if CISST_MTS_HAS_ICE
            if (remoteConnection) {
                mtsManagerLocalInterface * localManagerServer = GetProcessObject(serverProcessName);
                const std::string clientComponentProxyName = GetComponentProxyName(clientProcessName, clientComponentName);
                // Check if network proxy client is active. It is possible that
                // a local manager server is disconnected due to crashes or any 
                // other reason. This check prevents redundant error messages.
                mtsManagerProxyServer * proxyClient = dynamic_cast<mtsManagerProxyServer *>(localManagerServer);
                if (proxyClient->GetNetworkProxyClient(serverProcessName)) {
                    if (localManagerServer->RemoveRequiredInterfaceProxy(clientComponentProxyName, clientRequiredInterfaceName, serverProcessName)) {
                        // If no interface exists on the component proxy, it should be removed.
                        const int interfaceCount = localManagerServer->GetCurrentInterfaceCount(clientComponentProxyName, serverProcessName);
                        if (interfaceCount != -1) {
                            if (interfaceCount == 0) {
                                CMN_LOG_CLASS_RUN_VERBOSE <<"Disconnect: remove client component proxy with no active interface: " << clientComponentProxyName << std::endl;
                                if (!localManagerServer->RemoveComponentProxy(clientComponentProxyName, serverProcessName)) {
                                    CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to remove client component proxy: " 
                                        << clientComponentProxyName << " on " << serverProcessName << std::endl;
                                    return false;
                                }
                            }
                        }
                    } else {
                        CMN_LOG_CLASS_RUN_WARNING << "Disconnect: failed to update local component manager at server side" << std::endl;
                    }
                }
            }
#endif
        }
    }

    // Remove provided interfaces' connection information
    if (connectionMapOfProvidedInterface->size()) {
        // Get an element that contains connection information
        interfaceUID = GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName);
        connectionInfo = connectionMapOfProvidedInterface->GetItem(interfaceUID);        

        // Release allocated memory
        if (connectionInfo) {
            remoteConnection = connectionInfo->IsRemoteConnection();
            delete connectionInfo;
        }

        // Remove connection information
        if (connectionMapOfProvidedInterface->FindItem(interfaceUID)) {
            if (!connectionMapOfProvidedInterface->RemoveItem(interfaceUID)) {
                CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to update connection map in client process" << std::endl;
                return false;
            }

            // If the required interface is a proxy object (not an original interface), 
            // it should be removed when the connection is disconnected.
#if CISST_MTS_HAS_ICE
            if (remoteConnection) {
                mtsManagerLocalInterface * localManagerClient = GetProcessObject(clientProcessName);
                const std::string serverComponentProxyName = GetComponentProxyName(serverProcessName, serverComponentName);
                // Check if network proxy client is active. It is possible that
                // a local manager server is disconnected due to crashes or any 
                // other reason. This check prevents redundant error messages.
                mtsManagerProxyServer * proxyClient = dynamic_cast<mtsManagerProxyServer *>(localManagerClient);
                if (proxyClient->GetNetworkProxyClient(clientProcessName)) {
                    if (localManagerClient->RemoveProvidedInterfaceProxy(serverComponentProxyName, serverProvidedInterfaceName, clientProcessName)) {
                        // If no interface exists on the component proxy, it should be removed.
                        const int interfaceCount = localManagerClient->GetCurrentInterfaceCount(serverComponentProxyName, clientProcessName);
                        if (interfaceCount != -1) {
                            if (interfaceCount == 0) {
                                CMN_LOG_CLASS_RUN_VERBOSE <<"Disconnect: remove server component proxy with no active interface: " << serverComponentProxyName << std::endl;
                                if (!localManagerClient->RemoveComponentProxy(serverComponentProxyName, clientProcessName)) {
                                    CMN_LOG_CLASS_RUN_ERROR << "Disconnect: failed to remove server component proxy: " 
                                        << serverComponentProxyName << " on " << clientProcessName << std::endl;
                                    return false;
                                }
                            }
                        }
                    } else {
                        CMN_LOG_CLASS_RUN_WARNING << "Disconnect: failed to update local component manager at client side" << std::endl;
                    }
                }
            }
#endif
        }
    }

    // Remove connection element corresponding to this connection
    bool removed = false;
    const std::string clientInterfaceUID = GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName);
    const std::string serverInterfaceUID = GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName);
    ConnectionElement * element;
    ConnectionElementMapType::iterator it = ConnectionElementMap.begin();
    for (; it != ConnectionElementMap.end(); ++it) {
        element = it->second;
        if (clientInterfaceUID == GetInterfaceUID(element->ClientProcessName, element->ClientComponentName, element->ClientRequiredInterfaceName)) {
            if (serverInterfaceUID == GetInterfaceUID(element->ServerProcessName, element->ServerComponentName, element->ServerProvidedInterfaceName)) {
                ConnectionElementMapChange.Lock();
                delete element;
                ConnectionElementMap.erase(it);
                ConnectionElementMapChange.Unlock();
                
                removed = true;
                break;
            }
        }
    }

    if (!removed) {
        CMN_LOG_CLASS_RUN_VERBOSE << "Disconnect: failed to remove connection element: "
            << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName) << " - "
            << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName) << std::endl;
        return false;
    }

    CMN_LOG_CLASS_RUN_VERBOSE << "Disconnect: successfully disconnected: "
        << GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName) << " - "
        << GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName) << std::endl;

    return true;
}

mtsManagerGlobal::ConnectionMapType * mtsManagerGlobal::GetConnectionsOfProvidedInterface(
    const std::string & severProcessName, const std::string & serverComponentName, 
    const std::string & providedInterfaceName, InterfaceMapType ** interfaceMap)
{
    ComponentMapType * componentMap = ProcessMap.GetItem(severProcessName);
    if (componentMap == NULL) return NULL;

    *interfaceMap = componentMap->GetItem(serverComponentName);
    if (*interfaceMap == NULL) return NULL;

    ConnectionMapType * connectionMap = (*interfaceMap)->ProvidedInterfaceMap.GetItem(providedInterfaceName);

    return connectionMap;
}

mtsManagerGlobal::ConnectionMapType * mtsManagerGlobal::GetConnectionsOfProvidedInterface(
    const std::string & severProcessName, const std::string & serverComponentName, 
    const std::string & providedInterfaceName) const
{
    ComponentMapType * componentMap = ProcessMap.GetItem(severProcessName);
    if (componentMap == NULL) return NULL;

    InterfaceMapType * interfaceMap = componentMap->GetItem(serverComponentName);
    if (interfaceMap == NULL) return NULL;

    ConnectionMapType * connectionMap = interfaceMap->ProvidedInterfaceMap.GetItem(providedInterfaceName);

    return connectionMap;
}

mtsManagerGlobal::ConnectionMapType * mtsManagerGlobal::GetConnectionsOfRequiredInterface(
    const std::string & clientProcessName, const std::string & clientComponentName, 
    const std::string & requiredInterfaceName, InterfaceMapType ** interfaceMap)
{
    ComponentMapType * componentMap = ProcessMap.GetItem(clientProcessName);
    if (componentMap == NULL) return NULL;

    *interfaceMap = componentMap->GetItem(clientComponentName);
    if (*interfaceMap == NULL) return NULL;

    ConnectionMapType * connectionMap = (*interfaceMap)->RequiredInterfaceMap.GetItem(requiredInterfaceName);

    return connectionMap;
}

mtsManagerGlobal::ConnectionMapType * mtsManagerGlobal::GetConnectionsOfRequiredInterface(
    const std::string & clientProcessName, const std::string & clientComponentName, 
    const std::string & requiredInterfaceName) const
{
    ComponentMapType * componentMap = ProcessMap.GetItem(clientProcessName);
    if (componentMap == NULL) return NULL;

    InterfaceMapType * interfaceMap = componentMap->GetItem(clientComponentName);
    if (interfaceMap == NULL) return NULL;

    ConnectionMapType * connectionMap = interfaceMap->RequiredInterfaceMap.GetItem(requiredInterfaceName);

    return connectionMap;
}

bool mtsManagerGlobal::AddConnectedInterface(ConnectionMapType * connectionMap,
    const std::string & processName, const std::string & componentName,
    const std::string & interfaceName, const bool isRemoteConnection)
{
    if (!connectionMap) return false;

    ConnectedInterfaceInfo * connectedInterfaceInfo = 
        new ConnectedInterfaceInfo(processName, componentName, interfaceName, isRemoteConnection);

    std::string interfaceUID = GetInterfaceUID(processName, componentName, interfaceName);
    if (!connectionMap->AddItem(interfaceUID, connectedInterfaceInfo)) {
        CMN_LOG_CLASS_RUN_ERROR << "Cannot add peer interface's information: " 
            << GetInterfaceUID(processName, componentName, interfaceName) << std::endl;
        return false;
    }

    return true;
}

//-------------------------------------------------------------------------
//  Networking
//-------------------------------------------------------------------------
#if CISST_MTS_HAS_ICE
bool mtsManagerGlobal::StartServer()
{
    // Create an instance of mtsComponentInterfaceProxyServer
    ProxyServer = new mtsManagerProxyServer("ManagerServerAdapter", mtsManagerProxyServer::GetManagerCommunicatorID());

    // Run proxy server
    if (!ProxyServer->Start(this)) {
        CMN_LOG_CLASS_RUN_ERROR << "StartServer: Proxy failed to start: " << GetName() << std::endl;
        return false;
    }

    ProxyServer->GetLogger()->trace("mtsManagerGlobal", "Global component manager started.");

    // Register an instance of mtsComponentInterfaceProxyServer
    LocalManagerConnected = ProxyServer;

    return true;
}

//
// TODO: FIX THIS METHOD
//
void mtsManagerGlobal::SetIPAddress()
{
    // Fetch all ip addresses available on this machine.
    std::vector<std::string> ipAddresses;
    osaSocket::GetLocalhostIP(ipAddresses);

    // If there is only one ip address is detected, set it as this machine's ip address.
//    if (ipAddresses.size() == 1) {
        ProcessIP = ipAddresses[0];
    //} else {
    //    // If there are more than one ip address detected, wait for an user's input 
    //    // to decide what to use as an ip address of this machine.

    //    // Print out a list of all IP addresses detected
    //    std::cout << "\nList of IP addresses detected on this machine: " << std::endl;
    //    for (unsigned int i = 0; i < ipAddresses.size(); ++i) {
    //        std::cout << "\t" << i + 1 << ": " << ipAddresses[i] << std::endl;
    //    }

    //    // Wait for user's input
    //    char maxChoice = '1' + ipAddresses.size() - 1;
    //    int choice = 0;
    //    while (!('1' <= choice && choice <= maxChoice)) {
    //        std::cout << "\nChoose one to use: ";
    //        choice = cmnGetChar();
    //    }
    //    ProcessIP = ipAddresses[choice - '1'];

    //    std::cout << ProcessIP << std::endl;
    //}

    CMN_LOG_CLASS_INIT_VERBOSE << "SetIPAddress: This machine's IP address is " << ProcessIP << std::endl;
}

bool mtsManagerGlobal::SetProvidedInterfaceProxyAccessInfo(
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName,
    const std::string & endpointInfo)
{
    // Get a connection map of the provided interface at server side.
    ConnectionMapType * connectionMap = GetConnectionsOfProvidedInterface(
        serverProcessName, serverComponentName, serverProvidedInterfaceName);
    if (!connectionMap) {
        CMN_LOG_CLASS_RUN_ERROR << "SetProvidedInterfaceProxyAccessInfo: failed to get connection map: " 
            << serverProcessName << ":" << serverComponentName << ":" << serverProvidedInterfaceName << std::endl;
        return false;
    }

    // Get the information about the connected required interface
    const std::string requiredInterfaceUID = GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName);
    mtsManagerGlobal::ConnectedInterfaceInfo * connectedInterfaceInfo = connectionMap->GetItem(requiredInterfaceUID);
    if (!connectedInterfaceInfo) {
        CMN_LOG_CLASS_RUN_ERROR << "SetProvidedInterfaceProxyAccessInfo: failed to get connection information"
            << clientProcessName << ":" << clientComponentName << ":" << clientRequiredInterfaceName << std::endl;
        return false;
    }

    // Set server proxy access information
    connectedInterfaceInfo->SetProxyAccessInfo(endpointInfo);

    CMN_LOG_CLASS_RUN_VERBOSE << "SetProvidedInterfaceProxyAccessInfo: set proxy access info: " << endpointInfo << std::endl;

    return true;
}

bool mtsManagerGlobal::GetProvidedInterfaceProxyAccessInfo(
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName,
    std::string & endpointInfo)
{
    // Get a connection map of the provided interface at server side.
    ConnectionMapType * connectionMap = GetConnectionsOfProvidedInterface(
        serverProcessName, serverComponentName, serverProvidedInterfaceName);
    if (!connectionMap) {
        CMN_LOG_CLASS_RUN_ERROR << "GetProvidedInterfaceProxyAccessInfo: failed to get connection map: " 
            << serverProcessName << ":" << serverComponentName << ":" << serverProvidedInterfaceName << std::endl;
        return false;
    }

    // Get the information about the connected required interface
    mtsManagerGlobal::ConnectedInterfaceInfo * connectedInterfaceInfo;
    // If a client interface is not specified
    if (clientProcessName == "" && clientComponentName == "" && clientRequiredInterfaceName == "") {
        mtsManagerGlobal::ConnectionMapType::const_iterator itFirst = connectionMap->begin();
        if (itFirst == connectionMap->end()) {
            CMN_LOG_CLASS_RUN_ERROR << "GetProvidedInterfaceProxyAccessInfo: failed to get connection information (no data): "
                << mtsManagerGlobal::GetInterfaceUID(serverProcessName, serverComponentName, serverProvidedInterfaceName) << std::endl;
            return false;
        }
        connectedInterfaceInfo = itFirst->second;
    }
    // If a client interface is specified
    else {
        const std::string requiredInterfaceUID = GetInterfaceUID(clientProcessName, clientComponentName, clientRequiredInterfaceName);
        connectedInterfaceInfo = connectionMap->GetItem(requiredInterfaceUID);
    }

    if (!connectedInterfaceInfo) {
        CMN_LOG_CLASS_RUN_ERROR << "GetProvidedInterfaceProxyAccessInfo: failed to get connection information"
            << clientProcessName << ":" << clientComponentName << ":" << clientRequiredInterfaceName << std::endl;
        return false;
    }

    // Get server proxy access information
    endpointInfo = connectedInterfaceInfo->GetEndpointInfo();

    return true;
}
#endif

bool mtsManagerGlobal::InitiateConnect(const unsigned int connectionID,
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName)
{
    // Get local component manager that manages the client component.
    mtsManagerLocalInterface * localManagerClient = GetProcessObject(clientProcessName);
    if (!localManagerClient) {
        CMN_LOG_CLASS_RUN_ERROR << "InitiateConnect: Cannot find local component manager with client process: " << clientProcessName << std::endl;
        return false;
    }

    return localManagerClient->ConnectClientSideInterface(connectionID,
        clientProcessName, clientComponentName, clientRequiredInterfaceName,
        serverProcessName, serverComponentName, serverProvidedInterfaceName, clientProcessName);
}

bool mtsManagerGlobal::ConnectServerSideInterface(const unsigned int providedInterfaceProxyInstanceID,
    const std::string & clientProcessName, const std::string & clientComponentName, const std::string & clientRequiredInterfaceName,
    const std::string & serverProcessName, const std::string & serverComponentName, const std::string & serverProvidedInterfaceName)
{
    // Get local component manager that manages the server component.
    mtsManagerLocalInterface * localManagerServer = GetProcessObject(serverProcessName);
    if (!localManagerServer) {
        CMN_LOG_CLASS_RUN_ERROR << "ConnectServerSideInterface: Cannot find local component manager with server process: " << serverProcessName << std::endl;
        return false;
    }

    return localManagerServer->ConnectServerSideInterface(providedInterfaceProxyInstanceID,
        clientProcessName, clientComponentName, clientRequiredInterfaceName,
        serverProcessName, serverComponentName, serverProvidedInterfaceName, serverProcessName);
}
