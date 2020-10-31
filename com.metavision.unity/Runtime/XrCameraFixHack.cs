// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR;


/// <summary>
/// For use with URP (at least older than v10) and "Single Camera" stereo rendering mode:
/// Add this to the final (output) camera of your display pipeline to permit the "Single Camera"
/// stereo rendering mode in the plugin to work properly.
///
/// A null pointer error on gameplay "stop" is normal and can be ignored, unless you know how to fix it.
/// </summary>
[DisallowMultipleComponent]
[RequireComponent(typeof(Camera))]
public class XrCameraFixHack : MonoBehaviour
{

// Not needed with URP 10 and up
#if UNITY_URP_10PLUS

    void Start()
    {
        Debug.LogWarning("URP v10 or newer detected: can remove the XrCameraFixHack component from your final camera.");
    }

#else

    private Camera myCam;

    private bool gotOrig = false;
    private RenderTexture origTexture;

    private XRDisplaySubsystem display = null;

    private bool running = false;

    void Start()
    {
        PopulateDisplaySubsystem();
        running = true;
    }

    void Stop()
    {
        PopulateDisplaySubsystem();
        running = false;
    }

    private void PopulateDisplaySubsystem()
    {
        List<XRDisplaySubsystem> xrDisplayList = new List<XRDisplaySubsystem>();
        SubsystemManager.GetInstances(xrDisplayList);
        if (xrDisplayList.Count == 1)
        {
            if (display == null)
            {
                Debug.Log("Got single active XRDisplaySubsystem");
            }
            display = xrDisplayList[0];
        }
        else
        {
            if (display != null)
            {
                Debug.Log("Clearing cached XRDisplaySubsystem");
            }
            display = null;
        }
    }

    private void OnEnable()
    {
        myCam = GetComponent<Camera>();
        PopulateDisplaySubsystem();
        RenderPipelineManager.endCameraRendering += OnEndCamera;
        RenderPipelineManager.beginCameraRendering += OnBeginCamera;
    }

    private void OnDisable()
    {
        RenderPipelineManager.endCameraRendering -= OnEndCamera;
        RenderPipelineManager.beginCameraRendering -= OnBeginCamera;
        myCam = null;
        display = null;
    }

    private bool RenderingSingleCamera(Camera camera)
    {
        if (!running)
        {
            return false;
        }
        // when this camera is rendering
        if (myCam != camera)
        {
            return false;
        }

        // Our workaround is for single pass, single param rendering
        if (display == null)
        {
            return false;
        }
        if (display.GetRenderPassCount() != 1)
        {
            return false;
        }
        display.GetRenderPass(0, out var renderPass);
        return (renderPass.GetRenderParameterCount() == 1);
    }

    void OnBeginCamera(ScriptableRenderContext scriptable, Camera camera)
    {
        if (!RenderingSingleCamera(camera))
        {
            return;
        }
        if (!gotOrig)
        {
            gotOrig = true;
            origTexture = myCam.targetTexture;
        }
        // Not quite sure why this fixes things, but it does.
        // Idea from https://forum.unity.com/threads/rendering-to-vr-viewport-in-srp.756812/
        var texture = display.GetRenderTextureForRenderPass(0);
        myCam.targetTexture = texture;
    }

    void OnEndCamera(ScriptableRenderContext scriptable, Camera camera)
    {
        if (!RenderingSingleCamera(camera))
        {
            return;
        }
        if (gotOrig)
        {
            camera.targetTexture = origTexture;
            origTexture = null;
            gotOrig = false;
        }
    }
#endif

}
