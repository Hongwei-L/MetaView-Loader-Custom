using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.XR;
using UnityEngine.XR.Management;
using Unity.XR.MetaView;

/// <summary>
/// Add this to the final (output) camera of your display pipeline to set the FOV for the "single view"
/// stereo rendering mode in the plugin.
/// </summary>
[DisallowMultipleComponent]
[RequireComponent(typeof(Camera))]
public class ConfigureXrProjection : MonoBehaviour
{

    // [SerializeField]
    // public GameObject Panel;

    [Tooltip("The tangent of the left half-field-of-view (negative), aka frustum left extent at unit distance.")]
    [Range(-2.7f, -0.1f)]
    [SerializeField]
    public float Left = -1;

    [Tooltip("The tangent of the right half-field-of-view (positive), aka frustum right extent at unit distance.")]
    [Range(0.1f, 2.7f)]
    [SerializeField]
    public float Right = 1;

    [Tooltip("The tangent of the top half-field-of-view (positive), aka frustum top extent at unit distance.")]
    [Range(0.1f, 2.7f)]
    [SerializeField]
    public float Top = 1;

    [Tooltip("The tangent of the bottom half-field-of-view (negative), aka frustum bottom extent at unit distance.")]
    [Range(-2.7f, -0.1f)]
    [SerializeField]
    public float Bottom = -1;

    private MetaViewLoader loader = null;

    private Camera myCam;

    private bool callbackAdded = false;

    // Start is called before the first frame update
    void Start()
    {
        PopulateLoader();
    }

    void Stop()
    {
        loader = null;
        CleanupCallback();
    }

    private void OnEnable()
    {
        myCam = GetComponent<Camera>();
        callbackAdded = true;
        RenderPipelineManager.beginCameraRendering += OnBeginCamera;
    }
    private void OnDisable()
    {
        myCam = null;
        CleanupCallback();
    }

    private void CleanupCallback()
    {
        if (callbackAdded)
        {
            RenderPipelineManager.beginCameraRendering -= OnBeginCamera;
            callbackAdded = false;
        }

    }

    private void PopulateLoader()
    {
        if (loader == null)
        {
            loader = XRGeneralSettings.Instance.Manager.ActiveLoaderAs<MetaViewLoader>();
        }
    }

    void OnBeginCamera(ScriptableRenderContext scriptable, Camera camera)
    {
        // when this camera is rendering
        if (myCam != camera)
        {
            return;
        }
        PopulateLoader();
        if (loader == null)
        {
            Debug.Log($"{PluginMetadata.DebugLogPrefix}Could not get Meta View loader!");
            return;
        }
        // TODO Compute this using distance to Panel, etc.
        bool success = loader.SetupForSingleCamera(Left, Right, Top, Bottom);
        if (success)
        {
            CleanupCallback();
        }
    }
}
