using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class MainMenuUI : MonoBehaviour
{

    [SerializeField] private MainMenu mainMenu = null;

    [SerializeField] private GameObject startPanel = null;
    [SerializeField] private GameObject aboutPanel = null;

    private void OnEnable()
    {
        mainMenu.onStartPanel += StartPanel;
        mainMenu.onAboutPanel += AboutPanel;
    }

    void Start()
    {
        
    }

    private void StartPanel()
    {
        SetStartPanel(true);
    }

    private void AboutPanel()
    {
        SetStartPanel(false);
    }

    private void SetStartPanel(bool isStartPanelEnabledParam)
    {
        startPanel.SetActive(isStartPanelEnabledParam);
        aboutPanel.SetActive(!isStartPanelEnabledParam);
    }

    private void OnDisable()
    {
        mainMenu.onStartPanel -= StartPanel;
        mainMenu.onAboutPanel -= AboutPanel;
    }
}
