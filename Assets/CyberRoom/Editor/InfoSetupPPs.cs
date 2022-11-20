using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;

[InitializeOnLoad]
public class InfoSetupPPs : EditorWindow
{
    static bool showdialogwindow = true;
    static InfoSetupPPs dialogwindow;
    static string prefkey;

    static InfoSetupPPs()
    {
        EditorApplication.update += Update;
    }
    static void Update()
    {
        var datapath = Application.dataPath;
        var strval = datapath.Split("/"[0]);
        prefkey = strval[strval.Length - 2];

        showdialogwindow = (!EditorPrefs.HasKey(prefkey));
        if (showdialogwindow)
        {
            dialogwindow = GetWindow<InfoSetupPPs>(true);
            dialogwindow.minSize = new Vector2(430, 630);
        }
        EditorApplication.update -= Update;
    }

    public void OnGUI()
    {
        var rect = GUILayoutUtility.GetRect(position.width - 10, 100, GUI.skin.box);

        Texture2D ilranchlogo = AssetDatabase.LoadAssetAtPath<Texture2D>(
            Path.GetDirectoryName(AssetDatabase.GetAssetPath(MonoScript.FromScriptableObject(this))) + "/LogoDialog.png");
        if (ilranchlogo != null)
        {
            GUI.DrawTexture(rect, ilranchlogo, ScaleMode.ScaleToFit);
        }

        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("Camera Effects (Post Processing) setup for Beginners:", MessageType.Info, true);
        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("This package comes with Camera Effects. In order to enable it you should:", MessageType.None);
        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("1. Open package manager:", MessageType.None);

        GUI.backgroundColor = Color.green;
        if (GUILayout.Button("Open Package Manager"))
        {
            EditorApplication.ExecuteMenuItem("Window/Package Manager");
        }

        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("2. Install 'Post Processing':", MessageType.None);

        var rect2 = GUILayoutUtility.GetRect(position.width - 10, 200, GUI.skin.box);

        Texture2D ilralogo = AssetDatabase.LoadAssetAtPath<Texture2D>(
            Path.GetDirectoryName(AssetDatabase.GetAssetPath(MonoScript.FromScriptableObject(this))) + "/scr01.png");
        if (ilranchlogo != null)
        {
            GUI.DrawTexture(rect2, ilralogo, ScaleMode.ScaleToFit);
        }

        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("If you don't need use/install Post Processing, Just remove 'Missing Script' from Camera and 'Volume' GameObject from DemoScene", MessageType.None);

        EditorGUILayout.HelpBox("3. Open project settings:", MessageType.None);

        GUI.backgroundColor = Color.green;
        if (GUILayout.Button("Open Project Settings"))
        {
            EditorApplication.ExecuteMenuItem("Edit/Project Settings...");
        }
        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("Goto Player > Color Space,", MessageType.None);
        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("Switch color space to 'Linear'", MessageType.None);
        GUI.backgroundColor = Color.white;
        EditorGUILayout.HelpBox("Open 'Demo Scene'", MessageType.None);

        GUILayout.FlexibleSpace();
        GUI.backgroundColor = Color.cyan;
        if (GUILayout.Button("Close Prompt"))
        {
            EditorPrefs.SetBool(prefkey, true);
            Close();
        }
    }
}
