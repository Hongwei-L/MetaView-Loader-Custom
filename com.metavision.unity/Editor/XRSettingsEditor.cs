// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/com.valve.openvr/Editor/OpenVRSettingsEditor.cs
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using Unity.XR.MetaView;

namespace Unity.XR.MetaView.Editor
{
    [CustomEditor(typeof(Settings))]
    public class XRSettingsEditor : UnityEditor.Editor
    {
        private const string kStereoRenderingMode = "StereoRenderingMode";

        static GUIContent s_StereoRenderingMode = EditorGUIUtility.TrTextContent("Stereo Rendering Mode");

        private SerializedProperty m_StereoRenderingMode;

        private const string kMirrorViewModeKey = "MirrorView";

        static GUIContent s_MirrorViewMode = EditorGUIUtility.TrTextContent("Mirror View Mode");

        private SerializedProperty m_MirrorViewMode;

        private const string kRotateEyesKey = "RotateEyes";

        static GUIContent s_RotateEyes = EditorGUIUtility.TrTextContent("Rotate Eyes");

        private SerializedProperty m_RotateEyes;

        private const string kRenderGameViewKey = "RenderGameView";

        static GUIContent s_RenderGameView = EditorGUIUtility.TrTextContent("Render Game View");

        private SerializedProperty m_RenderGameView;

        /// <summary>
        /// Encapsulate/factor out a bunch of code used for each int property we check.
        /// </summary>
        internal class IntChangeChecker
        {
            private SerializedProperty serProp;
            private int defaultValue;
            private int origValue;

            private int currentValue
            {
                get => serProp == null ? defaultValue : serProp.intValue;
            }

            /// Constructor - saves original value
            internal IntChangeChecker(SerializedProperty prop, int defaultVal)
            {
                serProp = prop;
                defaultValue = defaultVal;
                origValue = currentValue;
            }

            internal delegate void HandleChange(int newValue);

            /// <summary>
            /// If change occurs while app is playing, the handler passed is invoked with the new value.
            /// </summary>
            /// <param name="handler">your desired handler</param>
            internal void LookForChange(HandleChange handler)
            {
                int newVal = currentValue;
                if (newVal != origValue && Application.isPlaying)
                {
                    handler(newVal);
                }
            }
        }

        public GUIContent WindowsTab;
        private int tab = 0;

        public void OnEnable()
        {
            WindowsTab = new GUIContent("", EditorGUIUtility.IconContent("BuildSettings.Standalone.Small").image);
        }

        private void PopulateSerializedPropertyIfNeeded(ref SerializedProperty prop, string key)
        {
            if (serializedObject == null || serializedObject.targetObject == null)
                return;

            if (prop == null)
            {
                prop = serializedObject.FindProperty(key);
            }
        }
        public override void OnInspectorGUI()
        {
            if (serializedObject == null || serializedObject.targetObject == null)
                return;

            PopulateSerializedPropertyIfNeeded(ref m_StereoRenderingMode, kStereoRenderingMode);
            PopulateSerializedPropertyIfNeeded(ref m_MirrorViewMode, kMirrorViewModeKey);
            PopulateSerializedPropertyIfNeeded(ref m_RotateEyes, kRotateEyesKey);
            PopulateSerializedPropertyIfNeeded(ref m_RenderGameView, kRenderGameViewKey);

            serializedObject.Update();

            var mirrorMode = new IntChangeChecker(m_MirrorViewMode, 0);
            var rotateEyes = new IntChangeChecker(m_RotateEyes, 1);
            var gameView = new IntChangeChecker(m_RenderGameView, 1);

            tab = GUILayout.Toolbar(tab, new GUIContent[] { WindowsTab }, EditorStyles.toolbarButton);
            EditorGUILayout.Space();

            EditorGUILayout.BeginVertical(GUILayout.ExpandWidth(true));
            if (tab == 0)
            {
                EditorGUILayout.PropertyField(m_StereoRenderingMode, s_StereoRenderingMode);
                if (m_MirrorViewMode != null)
                    EditorGUILayout.PropertyField(m_MirrorViewMode, s_MirrorViewMode);
                if (m_RotateEyes != null)
                    EditorGUILayout.PropertyField(m_RotateEyes, s_RotateEyes);
                if (m_RenderGameView != null)
                    EditorGUILayout.PropertyField(m_RenderGameView, s_RenderGameView);
            }
            EditorGUILayout.EndVertical();

            serializedObject.ApplyModifiedProperties();

            mirrorMode.LookForChange(
                newMode => Settings.SetMirrorViewMode((ushort)newMode));

            rotateEyes.LookForChange(
                newRotate => Settings.TellPluginRotateEyes((ushort)newRotate));

            gameView.LookForChange(
                newGameView => Settings.GetSettings().SetGameView((Settings.GameViewOptions)newGameView));
        }
    }
}
