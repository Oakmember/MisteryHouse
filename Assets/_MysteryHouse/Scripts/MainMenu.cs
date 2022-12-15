using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;

public class MainMenu : MonoBehaviour
{
    public Action onStartPanel;
    public Action onAboutPanel;

    [SerializeField]
    private string sceneName = null;
    
    void Start()
    {
        onStartPanel.Invoke();
    }

    IEnumerator LoadGameCor()
    {
        yield return new WaitForSeconds(5.0f);

        LoadGame();
    }

    public void LoadGame()
    {
        SceneManager.LoadScene(sceneName);
    }

    //////////////////////BUTTONS

    public void ButtonClick_Start()
    {
        StartCoroutine(LoadGameCor());
    }

    public void ButtonClick_About()
    {
        onAboutPanel.Invoke();
    }

    public void ButtonClick_Back()
    {
        onStartPanel.Invoke();
    }


}
