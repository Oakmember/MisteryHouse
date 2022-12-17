using Shared;
using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class SafeLeverController : MonoBehaviour
{
    public Action onSafeLeverDown;

    [SerializeField]
    private HingeJoint hingeJointRef = null;

    private bool isSafeLeverLocked = false;

    // Start is called before the first frame update
    void Start()
    {
        
    }

    public void SetSafeLeverEnabled()
    {
        gameObject.layer = LayerMask.NameToLayer(Consts.interactableLayer);
    }

    private void Update()
    {
        float turnValue = Mathf.Abs(hingeJointRef.angle);

        if (isSafeLeverLocked) return;

        if (turnValue > 80)
        {
            isSafeLeverLocked = true;
            onSafeLeverDown.Invoke();
        }
    }
}
