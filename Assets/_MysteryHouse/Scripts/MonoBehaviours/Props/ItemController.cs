using EPOOutline;
using Shared;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class ItemController : MonoBehaviour
{

    [SerializeField]
    private HighlightController highlightController = null;

    private Outlinable outlinable = null;

    public bool IsItemGrabbed { get; set; } = false;

    private void Awake()
    {
        outlinable = gameObject.AddComponent<Outlinable>();
    }

    void Start()
    {
        
    }

    public void OnSelectEvent()
    {
        highlightController.gameObject.SetActive(false);
        IsItemGrabbed = true;
        Debug.Log("Grab");
        outlinable.enabled = false;
    }

    public void OnDelectEvent()
    {
        highlightController.gameObject.SetActive(true);
        IsItemGrabbed = false;
        Debug.Log("Release");
        outlinable.enabled = true;
    }
}
